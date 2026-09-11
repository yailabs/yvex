/* Compile, persist, and reopen the immutable operator execution plan before model-open. */
#include <yvex/internal/compiler.h>

#include <yvex/internal/core.h>
#include <yvex/internal/decoder_plan.h>
#include <yvex/internal/moe.h>
#include <yvex/internal/operator_graph.h>
#include <yvex/internal/program.h>
#include <yvex/internal/program_physical.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/transformer.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_PLAN_SCHEMA_V2 2u
#define MODEL_PLAN_SCHEMA_V3 3u
#define MODEL_PLAN_SCHEMA_V4 4u
#define MODEL_PLAN_SCHEMA_V5 5u
#define MODEL_PLAN_SCHEMA_V6 6u
#define MODEL_PLAN_SCHEMA_V7 7u
#define MODEL_PLAN_MAX_LAYERS 65536ull

typedef struct {
    const unsigned char *data;
    size_t count, offset;
} model_plan_cursor;

struct yvex_compiled_model_plan {
    unsigned int schema;
    char operator_graph_identity[YVEX_SHA256_HEX_BYTES];
    yvex_program_tensor_plan *dense_ffn;
    yvex_program_physical *forward, *output, *final, *draft_final;
    yvex_decoder_plan *decoder;
    yvex_moe_plan *moe, *draft_moe;
    yvex_transformer_plan *transformer, *draft_transformer;
    yvex_runtime_logits_plan_summary output_head;
};

int yvex_compiled_graph_identities(
    const char *operator_graph_identity,
    const yvex_materialization_summary *materialization,
    const yvex_runtime_descriptor_summary *descriptor,
    const yvex_attention_summary *attention,
    const yvex_attention_summary *draft_attention,
    char semantic[YVEX_SHA256_HEX_CAP], char executable[YVEX_SHA256_HEX_CAP])
{
    char semantic_value[YVEX_SHA256_HEX_CAP] = {0};
    char executable_value[YVEX_SHA256_HEX_CAP] = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    if (semantic) semantic[0] = '\0';
    if (executable) executable[0] = '\0';
    if (!yvex_sha256_hex_valid(operator_graph_identity) || !materialization ||
        !descriptor || !attention || !semantic || !executable)
        return 0;
    yvex_core_text_copy(semantic_value, sizeof(semantic_value),
                        operator_graph_identity);
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.runtime.executable-graph.v2") ||
        !yvex_sha256_update_text(&hash, semantic_value) ||
        !yvex_sha256_update_text(&hash, descriptor->runtime_descriptor_identity) ||
        !yvex_sha256_update_text(&hash, materialization->plan_identity) ||
        !yvex_sha256_update_u64(&hash, attention->required_binding_count) ||
        !yvex_sha256_update_u64(&hash, attention->payload_bytes_bound) ||
        !yvex_sha256_update_u64(&hash, draft_attention
                                           ? draft_attention->required_binding_count
                                           : 0ull) ||
        !yvex_sha256_update_u64(&hash, draft_attention
                                           ? draft_attention->payload_bytes_bound
                                           : 0ull) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, executable_value);
    yvex_core_text_copy(semantic, YVEX_SHA256_HEX_CAP, semantic_value);
    yvex_core_text_copy(executable, YVEX_SHA256_HEX_CAP, executable_value);
    return 1;
}

static int model_plan_refuse(yvex_error *err, yvex_status status,
                             const char *reason)
{
    yvex_error_set(err, status, "runtime.model-plan", reason);
    return status;
}

static int plan_put_u64(yvex_core_bytes *bytes, unsigned long long value)
{
    unsigned char encoded[8];
    unsigned int index;
    for (index = 0u; index < 8u; ++index)
        encoded[index] = (unsigned char)(value >> (index * 8u));
    return yvex_core_bytes_append(bytes, encoded, sizeof(encoded));
}

static int plan_put_values(yvex_core_bytes *bytes,
                           const unsigned long long *values, size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index)
        if (!plan_put_u64(bytes, values[index])) return 0;
    return 1;
}

static int plan_put_double(yvex_core_bytes *bytes, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return plan_put_u64(bytes, bits);
}

static int plan_put_text(yvex_core_bytes *bytes, const char *text)
{
    size_t count = text ? strnlen(text, YVEX_SHA256_HEX_CAP) : 0u;
    return count < YVEX_SHA256_HEX_CAP && plan_put_u64(bytes, count) &&
           yvex_core_bytes_append(bytes, text, count);
}

static int plan_get_u64(model_plan_cursor *cursor, unsigned long long *value)
{
    unsigned long long decoded = 0ull;
    unsigned int index;
    if (!cursor || !value || cursor->offset > cursor->count ||
        cursor->count - cursor->offset < 8u)
        return 0;
    for (index = 0u; index < 8u; ++index)
        decoded |= (unsigned long long)cursor->data[cursor->offset + index]
                   << (index * 8u);
    cursor->offset += 8u;
    *value = decoded;
    return 1;
}

static int plan_get_values(model_plan_cursor *cursor,
                           unsigned long long *values, size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index)
        if (!plan_get_u64(cursor, &values[index])) return 0;
    return 1;
}

static int plan_get_double(model_plan_cursor *cursor, double *value)
{
    unsigned long long bits;
    if (!plan_get_u64(cursor, &bits)) return 0;
    memcpy(value, &bits, sizeof(*value));
    return 1;
}

static int plan_get_text(model_plan_cursor *cursor,
                         char output[YVEX_SHA256_HEX_CAP])
{
    unsigned long long count;
    if (!plan_get_u64(cursor, &count) || !count ||
        count >= YVEX_SHA256_HEX_CAP || cursor->offset > cursor->count ||
        count > cursor->count - cursor->offset)
        return 0;
    memset(output, 0, YVEX_SHA256_HEX_CAP);
    memcpy(output, cursor->data + cursor->offset, (size_t)count);
    cursor->offset += (size_t)count;
    return 1;
}

static int plan_get_optional_text(model_plan_cursor *cursor,
                                  char output[YVEX_SHA256_HEX_CAP])
{
    unsigned long long count;
    if (!cursor || !output || !plan_get_u64(cursor, &count) ||
        count >= YVEX_SHA256_HEX_CAP || cursor->offset > cursor->count ||
        count > cursor->count - cursor->offset)
        return 0;
    memset(output, 0, YVEX_SHA256_HEX_CAP);
    if (count)
        memcpy(output, cursor->data + cursor->offset, (size_t)count);
    cursor->offset += (size_t)count;
    return 1;
}

static int moe_summary_write(yvex_core_bytes *bytes,
                             const yvex_moe_plan_summary *summary)
{
    const unsigned long long values[] = {
        summary->schema_version, summary->tensor_scope,
        summary->family_adapter_id, summary->family_adapter_version,
        summary->layer_count, summary->hash_router_layer_count,
        summary->learned_router_layer_count, summary->routed_experts,
        summary->shared_experts, summary->experts_per_token,
        summary->required_binding_count, summary->expert_subview_count};
    return plan_put_values(bytes, values, sizeof(values) / sizeof(values[0])) &&
           plan_put_text(bytes, summary->artifact_identity) &&
           plan_put_text(bytes, summary->materialization_identity) &&
           plan_put_text(bytes, summary->logical_model_identity) &&
           plan_put_text(bytes, summary->runtime_numeric_identity) &&
           plan_put_text(bytes, summary->runtime_descriptor_identity) &&
           plan_put_text(bytes, summary->attention_plan_identity) &&
           plan_put_text(bytes, summary->moe_plan_identity);
}

static int moe_layer_write(yvex_core_bytes *bytes,
                           const yvex_moe_layer_plan *layer)
{
    const unsigned long long values[] = {
        layer->schema_version, layer->ordinal, layer->layer_index,
        layer->predictor_index, layer->tensor_scope, layer->router_class,
        layer->scoring, layer->topk_policy, layer->activation,
        layer->hidden_width, layer->residual_streams, layer->expanded_width,
        layer->mhc_mixing_rows, layer->mhc_sinkhorn_iterations,
        layer->routed_experts, layer->shared_experts,
        layer->experts_per_token, layer->expert_intermediate_width,
        layer->shared_intermediate_width, layer->hash_table_rows,
        layer->hash_table_columns, layer->correction_bias_width,
        (unsigned int)layer->requires_token_ids,
        (unsigned int)layer->requires_correction_bias,
        (unsigned int)layer->normalize_topk_probabilities};
    unsigned long long slot;
    if (!plan_put_values(bytes, values, sizeof(values) / sizeof(values[0])) ||
        !plan_put_double(bytes, layer->rms_epsilon) ||
        !plan_put_double(bytes, layer->mhc_epsilon) ||
        !plan_put_double(bytes, layer->mhc_post_multiplier) ||
        !plan_put_double(bytes, layer->routed_scaling_factor) ||
        !plan_put_double(bytes, layer->activation_limit))
        return 0;
    for (slot = 0ull; slot < YVEX_MOE_WEIGHT_COUNT; ++slot)
        if (!plan_put_u64(bytes, layer->tensor_ids[slot])) return 0;
    for (slot = 0ull; slot < YVEX_MOE_WEIGHT_COUNT; ++slot)
        if (!plan_put_u64(bytes, layer->qtypes[slot])) return 0;
    return plan_put_text(bytes, layer->layer_identity);
}

static int moe_plan_write(yvex_core_bytes *bytes, const yvex_moe_plan *plan)
{
    const yvex_moe_plan_summary *summary = yvex_moe_plan_summary_get(plan);
    unsigned long long index;
    if (!summary || !moe_summary_write(bytes, summary)) return 0;
    for (index = 0ull; index < summary->layer_count; ++index)
        if (!moe_layer_write(bytes, yvex_moe_plan_layer_at(plan, index))) return 0;
    return 1;
}

static int moe_summary_read(model_plan_cursor *cursor,
                            yvex_moe_plan_summary *summary)
{
    unsigned long long v[12];
    memset(summary, 0, sizeof(*summary));
    if (!plan_get_values(cursor, v, sizeof(v) / sizeof(v[0])) ||
        v[0] > UINT_MAX || v[1] > UINT_MAX)
        return 0;
    summary->schema_version = (unsigned int)v[0];
    summary->tensor_scope = (yvex_tensor_scope)v[1];
    summary->family_adapter_id = v[2];
    summary->family_adapter_version = v[3];
    summary->layer_count = v[4];
    summary->hash_router_layer_count = v[5];
    summary->learned_router_layer_count = v[6];
    summary->routed_experts = v[7];
    summary->shared_experts = v[8];
    summary->experts_per_token = v[9];
    summary->required_binding_count = v[10];
    summary->expert_subview_count = v[11];
    return plan_get_text(cursor, summary->artifact_identity) &&
           plan_get_text(cursor, summary->materialization_identity) &&
           plan_get_text(cursor, summary->logical_model_identity) &&
           plan_get_text(cursor, summary->runtime_numeric_identity) &&
           plan_get_text(cursor, summary->runtime_descriptor_identity) &&
           plan_get_text(cursor, summary->attention_plan_identity) &&
           plan_get_text(cursor, summary->moe_plan_identity);
}

static int moe_layer_read(model_plan_cursor *cursor, yvex_moe_layer_plan *layer)
{
    unsigned long long v[25], slot;
    memset(layer, 0, sizeof(*layer));
    if (!plan_get_values(cursor, v, sizeof(v) / sizeof(v[0])) ||
        v[0] > UINT_MAX || v[4] > UINT_MAX || v[5] > UINT_MAX ||
        v[6] > UINT_MAX || v[7] > UINT_MAX || v[8] > UINT_MAX ||
        v[22] > 1ull || v[23] > 1ull || v[24] > 1ull)
        return 0;
    layer->schema_version = (unsigned int)v[0];
    layer->ordinal = v[1];
    layer->layer_index = v[2];
    layer->predictor_index = v[3];
    layer->tensor_scope = (yvex_tensor_scope)v[4];
    layer->router_class = (yvex_moe_router_class)v[5];
    layer->scoring = (yvex_moe_scoring_policy)v[6];
    layer->topk_policy = (yvex_moe_topk_policy)v[7];
    layer->activation = (yvex_moe_activation)v[8];
    layer->hidden_width = v[9];
    layer->residual_streams = v[10];
    layer->expanded_width = v[11];
    layer->mhc_mixing_rows = v[12];
    layer->mhc_sinkhorn_iterations = v[13];
    layer->routed_experts = v[14];
    layer->shared_experts = v[15];
    layer->experts_per_token = v[16];
    layer->expert_intermediate_width = v[17];
    layer->shared_intermediate_width = v[18];
    layer->hash_table_rows = v[19];
    layer->hash_table_columns = v[20];
    layer->correction_bias_width = v[21];
    layer->requires_token_ids = (int)v[22];
    layer->requires_correction_bias = (int)v[23];
    layer->normalize_topk_probabilities = (int)v[24];
    if (!plan_get_double(cursor, &layer->rms_epsilon) ||
        !plan_get_double(cursor, &layer->mhc_epsilon) ||
        !plan_get_double(cursor, &layer->mhc_post_multiplier) ||
        !plan_get_double(cursor, &layer->routed_scaling_factor) ||
        !plan_get_double(cursor, &layer->activation_limit))
        return 0;
    for (slot = 0ull; slot < YVEX_MOE_WEIGHT_COUNT; ++slot)
        if (!plan_get_u64(cursor, &layer->tensor_ids[slot])) return 0;
    for (slot = 0ull; slot < YVEX_MOE_WEIGHT_COUNT; ++slot) {
        unsigned long long qtype;
        if (!plan_get_u64(cursor, &qtype) || qtype > UINT_MAX) return 0;
        layer->qtypes[slot] = (unsigned int)qtype;
    }
    return plan_get_text(cursor, layer->layer_identity);
}

static int moe_plan_read(model_plan_cursor *cursor, yvex_moe_plan **out,
                         yvex_error *err)
{
    yvex_moe_plan_summary summary;
    yvex_moe_layer_plan *layers = NULL;
    unsigned long long index;
    int rc;
    if (!moe_summary_read(cursor, &summary) || !summary.layer_count ||
        summary.layer_count > MODEL_PLAN_MAX_LAYERS ||
        summary.layer_count > SIZE_MAX / sizeof(*layers))
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled MoE summary is malformed");
    layers = (yvex_moe_layer_plan *)calloc((size_t)summary.layer_count,
                                            sizeof(*layers));
    if (!layers)
        return model_plan_refuse(err, YVEX_ERR_NOMEM,
                                 "compiled MoE directory allocation failed");
    for (index = 0ull; index < summary.layer_count; ++index)
        if (!moe_layer_read(cursor, &layers[index])) {
            free(layers);
            return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                     "compiled MoE layer is malformed");
        }
    rc = yvex_moe_plan_import(out, &summary, layers, err);
    free(layers);
    return rc;
}

static int transformer_summary_write(
    yvex_core_bytes *bytes, const yvex_transformer_plan_summary *summary)
{
    const unsigned long long values[] = {
        summary->schema_version, summary->tensor_scope,
        summary->family_adapter_id, summary->family_adapter_version,
        summary->layer_count, summary->hidden_width, summary->residual_streams,
        summary->expanded_width, summary->maximum_context,
        summary->vocabulary_size, summary->initial_policy,
        summary->final_policy, summary->sinkhorn_iterations};
    unsigned long long slot;
    if (!plan_put_values(bytes, values, sizeof(values) / sizeof(values[0])) ||
        !plan_put_double(bytes, summary->mhc_epsilon) ||
        !plan_put_double(bytes, summary->output_norm_epsilon) ||
        !plan_put_text(bytes, summary->artifact_identity) ||
        !plan_put_text(bytes, summary->materialization_identity) ||
        !plan_put_text(bytes, summary->logical_model_identity) ||
        !plan_put_text(bytes, summary->runtime_numeric_identity) ||
        !plan_put_text(bytes, summary->runtime_descriptor_identity) ||
        !plan_put_text(bytes, summary->attention_plan_identity) ||
        !plan_put_text(bytes, summary->moe_plan_identity) ||
        !plan_put_text(bytes, summary->transformer_plan_identity))
        return 0;
    for (slot = 0ull; slot < YVEX_TRANSFORMER_WEIGHT_COUNT; ++slot) {
        const yvex_transformer_weight_binding *weight = &summary->weights[slot];
        const unsigned long long fields[] = {
            weight->tensor_id, weight->row_width, weight->row_count,
            weight->encoded_bytes, weight->role, weight->tensor_scope,
            weight->layer_index, weight->predictor_index, weight->qtype};
        if (!plan_put_values(bytes, fields, sizeof(fields) / sizeof(fields[0])))
            return 0;
    }
    return 1;
}

static int transformer_plan_write(yvex_core_bytes *bytes,
                                  const yvex_transformer_plan *plan)
{
    const yvex_transformer_plan_summary *summary =
        yvex_transformer_plan_summary_get(plan);
    unsigned long long index;
    if (!summary || !transformer_summary_write(bytes, summary)) return 0;
    for (index = 0ull; index < summary->layer_count; ++index) {
        const yvex_transformer_layer_plan *layer =
            yvex_transformer_plan_layer_at(plan, index);
        const unsigned long long fields[] = {
            layer ? layer->ordinal : ULLONG_MAX,
            layer ? layer->layer_index : ULLONG_MAX,
            layer ? layer->predictor_index : ULLONG_MAX,
            layer ? layer->tensor_scope : ULLONG_MAX};
        if (!layer || !plan_put_values(bytes, fields,
                                       sizeof(fields) / sizeof(fields[0])) ||
            !plan_put_text(bytes, layer->moe_layer_identity) ||
            !plan_put_text(bytes, layer->layer_identity))
            return 0;
    }
    return 1;
}

static int transformer_summary_read(
    model_plan_cursor *cursor, yvex_transformer_plan_summary *summary)
{
    unsigned long long v[13], slot;
    memset(summary, 0, sizeof(*summary));
    if (!plan_get_values(cursor, v, sizeof(v) / sizeof(v[0])) ||
        v[0] > UINT_MAX || v[1] > UINT_MAX || v[10] > UINT_MAX ||
        v[11] > UINT_MAX)
        return 0;
    summary->schema_version = (unsigned int)v[0];
    summary->tensor_scope = (yvex_tensor_scope)v[1];
    summary->family_adapter_id = v[2];
    summary->family_adapter_version = v[3];
    summary->layer_count = v[4];
    summary->hidden_width = v[5];
    summary->residual_streams = v[6];
    summary->expanded_width = v[7];
    summary->maximum_context = v[8];
    summary->vocabulary_size = v[9];
    summary->initial_policy = (yvex_transformer_initial_policy)v[10];
    summary->final_policy = (yvex_transformer_final_policy)v[11];
    summary->sinkhorn_iterations = v[12];
    if (!plan_get_double(cursor, &summary->mhc_epsilon) ||
        !plan_get_double(cursor, &summary->output_norm_epsilon) ||
        !plan_get_text(cursor, summary->artifact_identity) ||
        !plan_get_text(cursor, summary->materialization_identity) ||
        !plan_get_text(cursor, summary->logical_model_identity) ||
        !plan_get_text(cursor, summary->runtime_numeric_identity) ||
        !plan_get_text(cursor, summary->runtime_descriptor_identity) ||
        !plan_get_text(cursor, summary->attention_plan_identity) ||
        !plan_get_text(cursor, summary->moe_plan_identity) ||
        !plan_get_text(cursor, summary->transformer_plan_identity))
        return 0;
    for (slot = 0ull; slot < YVEX_TRANSFORMER_WEIGHT_COUNT; ++slot) {
        unsigned long long f[9];
        yvex_transformer_weight_binding *weight = &summary->weights[slot];
        if (!plan_get_values(cursor, f, sizeof(f) / sizeof(f[0])) ||
            f[4] > UINT_MAX || f[5] > UINT_MAX || f[8] > UINT_MAX)
            return 0;
        weight->tensor_id = f[0];
        weight->row_width = f[1];
        weight->row_count = f[2];
        weight->encoded_bytes = f[3];
        weight->role = (yvex_tensor_role)f[4];
        weight->tensor_scope = (yvex_tensor_scope)f[5];
        weight->layer_index = f[6];
        weight->predictor_index = f[7];
        weight->qtype = (unsigned int)f[8];
    }
    return 1;
}

static int transformer_plan_read(model_plan_cursor *cursor,
                                 yvex_transformer_plan **out, yvex_error *err)
{
    yvex_transformer_plan_summary summary;
    yvex_transformer_layer_plan *layers = NULL;
    unsigned long long index;
    int rc;
    if (!transformer_summary_read(cursor, &summary) || !summary.layer_count ||
        summary.layer_count > MODEL_PLAN_MAX_LAYERS ||
        summary.layer_count > SIZE_MAX / sizeof(*layers))
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled transformer summary is malformed");
    layers = (yvex_transformer_layer_plan *)calloc(
        (size_t)summary.layer_count, sizeof(*layers));
    if (!layers)
        return model_plan_refuse(
            err, YVEX_ERR_NOMEM,
            "compiled transformer directory allocation failed");
    for (index = 0ull; index < summary.layer_count; ++index) {
        unsigned long long v[4];
        if (!plan_get_values(cursor, v, sizeof(v) / sizeof(v[0])) ||
            v[3] > UINT_MAX ||
            !plan_get_text(cursor, layers[index].moe_layer_identity) ||
            !plan_get_text(cursor, layers[index].layer_identity)) {
            free(layers);
            return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                     "compiled transformer layer is malformed");
        }
        layers[index].ordinal = v[0];
        layers[index].layer_index = v[1];
        layers[index].predictor_index = v[2];
        layers[index].tensor_scope = (yvex_tensor_scope)v[3];
    }
    rc = yvex_transformer_plan_import(out, &summary, layers, err);
    free(layers);
    return rc;
}

static int output_head_write(yvex_core_bytes *bytes,
                             const yvex_runtime_logits_plan_summary *summary)
{
    const unsigned long long values[] = {
        summary->schema_version, summary->producer_kind,
        summary->family_adapter_id, summary->family_adapter_version,
        summary->output_head_tensor_id, summary->row_width,
        summary->row_count, summary->row_bytes, summary->encoded_bytes,
        summary->vocabulary_size, summary->hidden_width, summary->role, summary->qtype,
        (unsigned int)summary->separate_output_head,
        (unsigned int)summary->output_head_bias};
    return yvex_output_head_plan_validate(summary, NULL) == YVEX_OK &&
           plan_put_values(bytes, values, sizeof(values) / sizeof(values[0])) &&
           plan_put_text(bytes, summary->artifact_identity) &&
           plan_put_text(bytes, summary->materialization_identity) &&
           plan_put_text(bytes, summary->logical_model_identity) &&
           plan_put_text(bytes, summary->runtime_numeric_identity) &&
           plan_put_text(bytes, summary->runtime_descriptor_identity) &&
           plan_put_text(bytes, summary->transformer_plan_identity) &&
           plan_put_text(bytes, summary->decoder_plan_identity) &&
           plan_put_text(bytes, summary->output_head_plan_identity);
}

static int output_head_read(model_plan_cursor *cursor,
                            yvex_runtime_logits_plan_summary *summary,
                            int legacy_layout, yvex_error *err)
{
    unsigned long long v[15] = {0};
    size_t value_count = legacy_layout ? 14u : 15u;
    memset(summary, 0, sizeof(*summary));
    if (!plan_get_values(cursor, v, value_count) || v[0] > UINT_MAX ||
        (!legacy_layout && v[1] > UINT_MAX) ||
        v[legacy_layout ? 10u : 11u] > UINT_MAX ||
        v[legacy_layout ? 11u : 12u] > UINT_MAX ||
        v[legacy_layout ? 12u : 13u] > 1ull ||
        v[legacy_layout ? 13u : 14u] > 1ull)
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled output-head fields are malformed");
    summary->schema_version = (unsigned int)v[0];
    summary->producer_kind = legacy_layout
        ? YVEX_EXECUTION_PLAN_TRANSFORMER : (yvex_execution_plan_kind)v[1];
    summary->family_adapter_id = v[legacy_layout ? 1u : 2u];
    summary->family_adapter_version = v[legacy_layout ? 2u : 3u];
    summary->output_head_tensor_id = v[legacy_layout ? 3u : 4u];
    summary->row_width = v[legacy_layout ? 4u : 5u];
    summary->row_count = v[legacy_layout ? 5u : 6u];
    summary->row_bytes = v[legacy_layout ? 6u : 7u];
    summary->encoded_bytes = v[legacy_layout ? 7u : 8u];
    summary->vocabulary_size = v[legacy_layout ? 8u : 9u];
    summary->hidden_width = v[legacy_layout ? 9u : 10u];
    summary->role = (yvex_tensor_role)v[legacy_layout ? 10u : 11u];
    summary->qtype = (unsigned int)v[legacy_layout ? 11u : 12u];
    summary->separate_output_head = (int)v[legacy_layout ? 12u : 13u];
    summary->output_head_bias = (int)v[legacy_layout ? 13u : 14u];
    if (!plan_get_text(cursor, summary->artifact_identity) ||
        !plan_get_text(cursor, summary->materialization_identity) ||
        !plan_get_text(cursor, summary->logical_model_identity) ||
        !plan_get_text(cursor, summary->runtime_numeric_identity) ||
        !plan_get_text(cursor, summary->runtime_descriptor_identity) ||
        !(legacy_layout
              ? plan_get_text(cursor, summary->transformer_plan_identity)
              : plan_get_optional_text(
                    cursor, summary->transformer_plan_identity)) ||
        (!legacy_layout &&
         !plan_get_optional_text(cursor, summary->decoder_plan_identity)) ||
        !plan_get_text(cursor, summary->output_head_plan_identity))
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled output-head identities are malformed");
    return yvex_output_head_plan_validate(summary, err);
}

void yvex_compiled_model_plan_close(yvex_compiled_model_plan **owner)
{
    yvex_compiled_model_plan *plans = owner ? *owner : NULL;
    if (!plans) return;
    yvex_program_tensor_close(&plans->dense_ffn);
    yvex_program_physical_close(&plans->forward);
    yvex_program_physical_close(&plans->output);
    yvex_program_physical_close(&plans->final);
    yvex_program_physical_close(&plans->draft_final);
    yvex_decoder_plan_close(&plans->decoder);
    yvex_transformer_plan_close(&plans->draft_transformer);
    yvex_transformer_plan_close(&plans->transformer);
    yvex_moe_plan_close(&plans->draft_moe);
    yvex_moe_plan_close(&plans->moe);
    memset(plans, 0, sizeof(*plans));
    free(plans);
    *owner = NULL;
}

static int decoder_capabilities_unsupported(
    const yvex_runtime_capabilities *capabilities)
{
    return !capabilities || capabilities->moe_plan_ready ||
           capabilities->moe_router_ready ||
           capabilities->moe_routed_expert_ready ||
           capabilities->moe_shared_expert_ready ||
           capabilities->moe_block_ready || capabilities->transformer_ready ||
           capabilities->logits_cpu_ready || capabilities->logits_cuda_ready ||
           capabilities->logits_prefill_ready || capabilities->logits_decode_ready ||
           capabilities->logits_full_vocabulary_ready ||
           capabilities->logits_hidden_contract_ready ||
           capabilities->logits_partial_progress_ready ||
           capabilities->logits_ready || capabilities->generation_ready;
}

/* Import authenticated v3/v4 FFN semantics once at the schema boundary. Native
 * compilation supplies its real program. This lineage names a legacy compiled
 * projection, not an upstream/provider-source conformance proof. */
static int legacy_decoder_ffn_import(yvex_program_tensor_plan **out, const yvex_decoder_plan *decoder,
                                      yvex_error *err)
{
    const yvex_decoder_plan_summary *s = yvex_decoder_plan_summary_get(decoder);
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *module = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "sequence", .minimum = 1u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u};
    yvex_ir_id dimension, hidden, intermediate, signature[4], function, block, op;
    yvex_ir_id args[4], values[4], operands[2];
    size_t i;
    int rc;
    if (!s) return model_plan_refuse(err, YVEX_ERR_FORMAT, "legacy FFN import requires an admitted decoder");
    rows.maximum = s->maximum_context;
    rc = yvex_ir_module_open(&module, "legacy_decoder_ffn", s->decoder_plan_identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(module, &rows, &dimension, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dimension, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, s->hidden_width};
        rc = yvex_ir_type_intern(module, &t, &hidden, err);
    }
    if (rc == YVEX_OK) {
        signature[0] = hidden;
        t.shape[1].extent = s->intermediate_width;
        rc = yvex_ir_type_intern(module, &t, &intermediate, err);
    }
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, s->intermediate_width};
        t.shape[1].extent = s->hidden_width;
        rc = yvex_ir_type_intern(module, &t, &signature[1], err);
    }
    if (rc == YVEX_OK) {
        signature[2] = signature[1];
        t.shape[0].extent = s->hidden_width;
        t.shape[1].extent = s->intermediate_width;
        rc = yvex_ir_type_intern(module, &t, &signature[3], err);
    }
    if (rc == YVEX_OK)
        rc = yvex_ir_function_add(module, "dense_ffn", signature, 4u, &hidden, 1u, 0u, &function, err);
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
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &values[3], .operand_count = 1u};
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(module, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, err);
    if (rc == YVEX_OK) rc = yvex_program_tensor_compile(out, execution, "dense_ffn", err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&module);
    return rc;
}

static int compiled_forward_signature_valid(const yvex_compiled_model_plan *plan)
{
    const yvex_decoder_plan_summary *d = yvex_decoder_plan_summary_get(plan->decoder);
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(plan->forward);
    const yvex_program_physical_value *tokens, *position, *output;
    size_t i, delta = 0u, attention = 0u, embeddings = 0u;
    if (!s || !d || strcmp(s->entry, "forward") || s->input_count < 2u ||
        s->minimum_rows != 1u || s->maximum_rows != d->maximum_context || s->row_multiple != 1u ||
        s->input_count != 2u + 2u * d->recurrent_layer_count + d->attention_layer_count ||
        s->result_count != s->input_count - 1u) return 0;
    tokens = yvex_program_physical_value_at(plan->forward, 0u);
    position = yvex_program_physical_value_at(plan->forward, 1u);
    output = yvex_program_physical_value_at(plan->forward, yvex_program_physical_result_at(plan->forward, 0u));
    if (tokens->type.kind != YVEX_IR_TENSOR || tokens->type.scalar != YVEX_IR_INDEX || tokens->type.rank != 1u ||
        position->type.kind != YVEX_IR_SCALAR || position->type.scalar != YVEX_IR_INDEX ||
        output->type.kind != YVEX_IR_TENSOR || output->type.scalar != YVEX_IR_BF16 || output->type.rank != 2u ||
        output->type.shape[1].extent != d->hidden_width) return 0;
    for (i = 2u; i < s->input_count; ++i) {
        const yvex_program_physical_value *state = yvex_program_physical_value_at(plan->forward, i);
        if (state->type.kind != YVEX_IR_STATE) return 0;
    }
    for (i = 0u; i < s->step_count; ++i) {
        const yvex_program_physical_step *step = yvex_program_physical_step_at(plan->forward, i);
        delta += !strcmp(step->implementation, "gated_delta.bf16.f32state.v1");
        attention += !strcmp(step->implementation, "gated_causal.bf16.v1");
        if (!strcmp(step->implementation, "embedding.bf16.v1")) {
            const yvex_program_physical_value *weight =
                yvex_program_physical_value_at(plan->forward, step->operands[1]);
            if (step->operands[0] != 0u || weight->type.shape[0].extent != d->vocabulary_size ||
                weight->type.shape[1].extent != d->hidden_width) return 0;
            embeddings++;
        }
    }
    /* These persisted compatibility/report views may not contradict the program.
     * They are not used to construct its operations or parameter bindings. */
    return embeddings == 1u && delta == d->recurrent_layer_count && attention == d->attention_layer_count &&
        delta + attention == d->layer_count;
}

static int compiled_output_program_valid(const yvex_compiled_model_plan *plan)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(plan->output);
    const yvex_program_physical_summary *forward = yvex_program_physical_summary_get(plan->forward);
    const yvex_decoder_plan_summary *d = yvex_decoder_plan_summary_get(plan->decoder);
    const yvex_transformer_plan_summary *t = yvex_transformer_plan_summary_get(plan->transformer);
    unsigned long long rows = d ? d->maximum_context : t ? t->maximum_context : 0u;
    if (!plan->output_head.schema_version) return !s;
    if (!s) return plan->schema < MODEL_PLAN_SCHEMA_V7;
    if (s->minimum_rows != 1u || s->row_multiple != 1u || s->maximum_rows != rows ||
        yvex_output_head_program_validate(plan->output, &plan->output_head, NULL, NULL) != YVEX_OK) return 0;
    return plan->schema < MODEL_PLAN_SCHEMA_V7 || (forward &&
        !strcmp(s->semantic_identity, forward->semantic_identity) &&
        !strcmp(s->execution_identity, forward->execution_identity));
}

static int compiled_ffn_signature_valid(const yvex_compiled_model_plan *plan)
{
    const yvex_decoder_plan_summary *d = yvex_decoder_plan_summary_get(plan->decoder);
    const yvex_program_tensor_summary *p = yvex_program_tensor_summary_get(plan->dense_ffn);
    const yvex_program_tensor_value *a, *gate, *up, *down, *output;
    if (plan->schema >= MODEL_PLAN_SCHEMA_V6) return !p && compiled_forward_signature_valid(plan);
    if (!d) return !p;
    if (!p || p->input_count != 4u || p->result_count != 1u || p->minimum_rows != 1u ||
        p->maximum_rows != d->maximum_context || p->row_multiple != 1u) return 0;
    a = yvex_program_tensor_value_at(plan->dense_ffn, 0u);
    gate = yvex_program_tensor_value_at(plan->dense_ffn, 1u);
    up = yvex_program_tensor_value_at(plan->dense_ffn, 2u);
    down = yvex_program_tensor_value_at(plan->dense_ffn, 3u);
    output = yvex_program_tensor_value_at(plan->dense_ffn, yvex_program_tensor_result_at(plan->dense_ffn, 0u));
    return !a->parameter && a->width == d->hidden_width && gate->parameter && up->parameter && down->parameter &&
        gate->width == d->hidden_width && up->width == d->hidden_width &&
        gate->rows == d->intermediate_width && up->rows == d->intermediate_width &&
        down->width == d->intermediate_width && down->rows == d->hidden_width &&
        !output->parameter && output->width == d->hidden_width;
}

static int compiled_forward_build(yvex_compiled_model_plan *plan, const yvex_compiled_model_plan_request *r,
                                   yvex_error *err)
{
    const yvex_physical_execution_summary *physical =
        yvex_physical_execution_ir_summary(r->program_physical_parameters);
    const yvex_ir_module *m = yvex_program_parameters_module(r->program_parameters);
    const yvex_ir_module *program = yvex_program_execution_module(r->program);
    yvex_program_parameter_binding *bindings;
    size_t count = yvex_program_parameters_count(r->program_parameters), i;
    unsigned long long j;
    int rc = YVEX_OK;
    if (!program) return YVEX_OK; /* Historical schema import, never a native source projection. */
    if (!m || !physical || !count || strcmp(yvex_ir_identity(m), yvex_ir_identity(program)))
        return model_plan_refuse(err, YVEX_ERR_FORMAT, "program parameters require the exact compiled IR lineage");
    bindings = calloc(count, sizeof(*bindings));
    if (!bindings) return model_plan_refuse(err, YVEX_ERR_NOMEM, "physical parameter binding allocation failed");
    for (i = 0u; rc == YVEX_OK && i < count; ++i) {
        const yvex_program_parameter *parameter = yvex_program_parameter_at(r->program_parameters, i);
        size_t matches = 0u;
        for (j = 0u; j < physical->decision_count; ++j) {
            const yvex_physical_execution_decision *d =
                yvex_physical_execution_ir_decision_at(r->program_physical_parameters, j);
            if (d->terminal_tensor_id != parameter->terminal) continue;
            bindings[i] = (yvex_program_parameter_binding){parameter->value, d->terminal_tensor_id, d->canonical_qtype};
            matches++;
        }
        if (matches != 1u)
            rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "program parameter has ambiguous physical work");
    }
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&plan->forward, r->program, "forward", bindings, count,
        physical->identity, err);
    if (rc == YVEX_OK)
        rc = yvex_program_physical_parameters_validate(plan->forward, r->program_physical_parameters, err);
    if (rc == YVEX_OK && !compiled_forward_signature_valid(plan))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "compiled forward contradicts the retained producer view");
    if (rc == YVEX_OK && plan->output_head.schema_version)
        rc = yvex_program_physical_compile(&plan->output, r->program, "output", bindings, count,
            physical->identity, err);
    if (rc == YVEX_OK && plan->output)
        rc = yvex_output_head_program_validate(plan->output, &plan->output_head, r->program_physical_parameters, err);
    free(bindings);
    if (rc == YVEX_OK) {
        yvex_program_tensor_close(&plan->dense_ffn);
        plan->schema = MODEL_PLAN_SCHEMA_V7;
    }
    return rc;
}

static int compiled_ffn_build(yvex_compiled_model_plan *plan, const yvex_compiled_model_plan_request *r,
                               yvex_error *err)
{
    const yvex_ir_module *m = yvex_semantic_model_ir_program(r->semantic_model);
    int rc;
    if (m) {
        const char *id = yvex_ir_identity(yvex_program_execution_module(r->program));
        if (!id || strcmp(yvex_ir_identity(m), id))
            return model_plan_refuse(err, YVEX_ERR_FORMAT, "native model-plan requires matching execution IR");
        rc = yvex_program_tensor_compile(&plan->dense_ffn, r->program, "dense_ffn", err);
    } else rc = legacy_decoder_ffn_import(&plan->dense_ffn, plan->decoder, err);
    if (rc == YVEX_OK && !compiled_ffn_signature_valid(plan))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "compiled FFN signature differs from admitted operands");
    if (rc == YVEX_OK) plan->schema = MODEL_PLAN_SCHEMA_V5;
    return rc;
}

int yvex_compiled_model_plan_build(
    yvex_compiled_model_plan **out,
    const yvex_compiled_model_plan_request *request, yvex_error *err)
{
    yvex_compiled_model_plan *plan = NULL;
    const yvex_semantic_model_ir_summary *semantic = NULL;
    const yvex_decoder_plan_summary *decoder = NULL;
    int compile_execution;
    int rc;
    if (out) *out = NULL;
    if (!out || !request || !request->materialization ||
        !request->operator_graph || !request->descriptor ||
        !request->attention || !request->graph)
        return model_plan_refuse(err, YVEX_ERR_INVALID_ARG,
                                 "compiled model-plan inputs are required");
    plan = (yvex_compiled_model_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return model_plan_refuse(err, YVEX_ERR_NOMEM,
                                 "compiled model-plan allocation failed");
    plan->schema = MODEL_PLAN_SCHEMA_V4;
    {
        const yvex_operator_graph_summary *operators =
            yvex_operator_graph_ir_summary(request->operator_graph);
        const yvex_runtime_descriptor_summary *descriptor =
            yvex_runtime_descriptor_summary_get(request->descriptor);
        const yvex_attention_summary *attention =
            yvex_attention_plan_summary(request->attention);
        const yvex_attention_summary *draft =
            yvex_attention_plan_summary(request->draft_attention);
        semantic = yvex_semantic_model_ir_summary_get(request->semantic_model);
        if (!operators || !descriptor || !attention ||
            operators->family_adapter_id != request->family_adapter_id ||
            operators->family_adapter_version != request->family_adapter_version ||
            ((!semantic ||
              semantic->schema_version != YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2) &&
             operators->target_layer_count != attention->layer_count) ||
            (semantic &&
             semantic->schema_version == YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2 &&
             (operators->target_layer_count != semantic->decoder_layer_count ||
              attention->layer_count != semantic->attention_layer_count)) ||
            operators->draft_layer_count != (draft ? draft->layer_count : 0ull) ||
            operators->maximum_context !=
                descriptor->model_execution.maximum_context) {
            yvex_compiled_model_plan_close(&plan);
            return model_plan_refuse(
                err, YVEX_ERR_FORMAT,
                "operator graph does not match the compiled execution inputs");
        }
        yvex_core_text_copy(plan->operator_graph_identity,
                            sizeof(plan->operator_graph_identity),
                            operators->identity);
    }
    if (semantic &&
        semantic->schema_version == YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2) {
        rc = yvex_decoder_plan_compile(
            &plan->decoder, request->semantic_model,
            request->operator_graph, err);
        decoder = rc == YVEX_OK
                      ? yvex_decoder_plan_summary_get(plan->decoder) : NULL;
        if (rc != YVEX_OK || !decoder ||
            decoder->attention_layer_count !=
                yvex_attention_plan_summary(request->attention)->layer_count) {
            if (rc == YVEX_OK)
                rc = model_plan_refuse(
                    err, YVEX_ERR_FORMAT,
                    "decoder and attention plans have different populations");
            yvex_compiled_model_plan_close(&plan);
            return rc;
        }
        if (request->draft_attention ||
            decoder_capabilities_unsupported(&request->capabilities) ||
            request->capabilities.output_head_binding_ready !=
                request->capabilities.output_head_projection_ready) {
            yvex_compiled_model_plan_close(&plan);
            return model_plan_refuse(
                err, YVEX_ERR_FORMAT,
                "hybrid decoder capabilities claim unavailable execution");
        }
        if (request->capabilities.output_head_projection_ready)
            rc = yvex_output_head_plan_build_decoder(
                &plan->output_head, request->family_adapter_id,
                request->family_adapter_version, request->materialization,
                request->descriptor, plan->decoder,
                &request->logits_policy, err);
        else
            rc = YVEX_OK;
        if (rc == YVEX_OK) rc = compiled_ffn_build(plan, request, err);
        if (rc == YVEX_OK) rc = compiled_forward_build(plan, request, err);
        if (rc == YVEX_OK) {
            *out = plan;
            yvex_error_clear(err);
        } else {
            yvex_compiled_model_plan_close(&plan);
        }
        return rc;
    }
    compile_execution = request->capabilities.moe_plan_ready ||
                        request->capabilities.transformer_ready ||
                        request->capabilities.logits_ready;
    if (!compile_execution) {
        *out = plan;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!request->capabilities.moe_plan_ready ||
        !request->capabilities.transformer_ready ||
        !request->capabilities.logits_ready) {
        yvex_compiled_model_plan_close(&plan);
        return model_plan_refuse(
            err, YVEX_ERR_FORMAT,
            "partial compiled execution capabilities are inconsistent");
    }
    if (!request->graph->moe) {
        yvex_compiled_model_plan_close(&plan);
        return model_plan_refuse(
            err, YVEX_ERR_FORMAT,
            "compiled Transformer execution requires an MoE graph compiler");
    }
    rc = yvex_moe_plan_build(
        &plan->moe, request->graph->moe, request->family_adapter_id,
        request->family_adapter_version, request->materialization,
        request->descriptor, request->attention, err);
    if (rc == YVEX_OK)
        rc = yvex_transformer_plan_compile(
            &plan->transformer, &request->transformer_policy,
            request->family_adapter_id, request->family_adapter_version,
            request->materialization, request->descriptor,
            request->attention, plan->moe, YVEX_TENSOR_SCOPE_GLOBAL, err);
    if (rc == YVEX_OK)
        rc = yvex_output_head_plan_build_transformer(
            &plan->output_head, request->family_adapter_id,
            request->family_adapter_version, request->materialization,
            request->descriptor, plan->transformer,
            &request->logits_policy, err);
    if (rc == YVEX_OK && request->draft_attention)
        rc = yvex_moe_plan_build(
            &plan->draft_moe, request->graph->moe, request->family_adapter_id,
            request->family_adapter_version, request->materialization,
            request->descriptor, request->draft_attention, err);
    if (rc == YVEX_OK && request->draft_attention)
        rc = yvex_transformer_plan_compile(
            &plan->draft_transformer, &request->transformer_policy,
            request->family_adapter_id, request->family_adapter_version,
            request->materialization, request->descriptor,
            request->draft_attention, plan->draft_moe,
            YVEX_TENSOR_SCOPE_DRAFT, err);
    if (rc == YVEX_OK) *out = plan;
    else yvex_compiled_model_plan_close(&plan);
    return rc;
}

static int compiled_output_matches_producer(
    const yvex_compiled_model_plan *plans)
{
    const yvex_transformer_plan_summary *transformer;
    const yvex_decoder_plan_summary *decoder;
    const yvex_runtime_logits_plan_summary *output;
    if (!plans) return 0;
    transformer = yvex_transformer_plan_summary_get(plans->transformer);
    decoder = yvex_decoder_plan_summary_get(plans->decoder);
    output = plans->output_head.schema_version ? &plans->output_head : NULL;
    if (transformer)
        return output && !decoder &&
               output->producer_kind == YVEX_EXECUTION_PLAN_TRANSFORMER &&
               strcmp(output->transformer_plan_identity,
                      transformer->transformer_plan_identity) == 0;
    if (decoder)
        return !output ||
               (output->producer_kind == YVEX_EXECUTION_PLAN_DECODER &&
                strcmp(output->decoder_plan_identity,
                       decoder->decoder_plan_identity) == 0);
    return !output;
}

int yvex_compiled_model_plan_encode(
    const yvex_compiled_model_plan *plans, yvex_core_bytes *bytes,
    yvex_error *err)
{
    int target_present = plans && plans->moe && plans->transformer;
    int draft_present = plans && plans->draft_moe && plans->draft_transformer;
    int decoder_present = plans && plans->decoder;
    int output_present = plans && plans->output_head.schema_version;
    if (!plans || !bytes ||
        (plans->moe != NULL) != (plans->transformer != NULL) ||
        (plans->draft_moe != NULL) != (plans->draft_transformer != NULL) ||
        (target_present && decoder_present) || (!target_present && draft_present) ||
        !compiled_output_matches_producer(plans) || !compiled_output_program_valid(plans) ||
        !compiled_ffn_signature_valid(plans) ||
        !yvex_sha256_hex_valid(plans->operator_graph_identity) ||
        !plan_put_text(bytes, plans->schema == MODEL_PLAN_SCHEMA_V7 ? "yvex.compiled-model-plan.v7" :
                       plans->schema == MODEL_PLAN_SCHEMA_V6 ? "yvex.compiled-model-plan.v6" :
                       plans->schema == MODEL_PLAN_SCHEMA_V5 ?
                       "yvex.compiled-model-plan.v5" : "yvex.compiled-model-plan.v4") ||
        !plan_put_u64(bytes, plans->schema >= MODEL_PLAN_SCHEMA_V5 ? plans->schema : MODEL_PLAN_SCHEMA_V4) ||
        !plan_put_text(bytes, plans->operator_graph_identity) ||
        !plan_put_u64(bytes, (unsigned int)target_present) ||
        (target_present &&
         (!moe_plan_write(bytes, plans->moe) ||
          !transformer_plan_write(bytes, plans->transformer))) ||
        !plan_put_u64(bytes, (unsigned int)draft_present) ||
        (draft_present &&
         (!moe_plan_write(bytes, plans->draft_moe) ||
          !transformer_plan_write(bytes, plans->draft_transformer))) ||
        !plan_put_u64(bytes, (unsigned int)decoder_present) ||
        (decoder_present &&
         yvex_decoder_plan_encode(plans->decoder, bytes, err) != YVEX_OK) ||
        !plan_put_u64(bytes, (unsigned int)output_present) ||
        (output_present && !output_head_write(bytes, &plans->output_head)))
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled model-plan encoding failed");
    if (plans->schema == MODEL_PLAN_SCHEMA_V5) {
        yvex_core_bytes program = {.maximum = 16u * 1024u * 1024u};
        int rc = yvex_program_tensor_encode(plans->dense_ffn, &program, err);
        if (rc == YVEX_OK && (!plan_put_u64(bytes, program.count) ||
            !yvex_core_bytes_append(bytes, program.data, program.count)))
            rc = model_plan_refuse(err, YVEX_ERR_NOMEM, "compiled tensor program encoding failed");
        free(program.data);
        if (rc != YVEX_OK) return rc;
    }
    if (plans->schema >= MODEL_PLAN_SCHEMA_V6) {
        yvex_core_bytes program = {.maximum = 256u * 1024u * 1024u};
        int rc = yvex_program_physical_encode(plans->forward, &program, err);
        if (rc == YVEX_OK && (!plan_put_u64(bytes, program.count) ||
            !yvex_core_bytes_append(bytes, program.data, program.count)))
            rc = model_plan_refuse(err, YVEX_ERR_NOMEM, "compiled forward program encoding failed");
        free(program.data);
        if (rc != YVEX_OK) return rc;
    }
    if (plans->schema == MODEL_PLAN_SCHEMA_V7) {
        yvex_core_bytes program = {.maximum = 256u * 1024u * 1024u};
        int rc = plans->output ? yvex_program_physical_encode(plans->output, &program, err) : YVEX_OK;
        if (rc == YVEX_OK && (!plan_put_u64(bytes, program.count) ||
            (program.count && !yvex_core_bytes_append(bytes, program.data, program.count))))
            rc = model_plan_refuse(err, YVEX_ERR_NOMEM, "compiled output program encoding failed");
        free(program.data);
        if (rc != YVEX_OK) return rc;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_compiled_model_plan_decode(
    yvex_compiled_model_plan **out, const unsigned char *data, size_t count,
    yvex_error *err)
{
    model_plan_cursor cursor = {data, count, 0u};
    yvex_compiled_model_plan *plan = NULL;
    char domain[YVEX_SHA256_HEX_CAP];
    unsigned long long schema, target_present, draft_present;
    unsigned long long decoder_present = 0ull, output_present = 0ull;
    unsigned long long expected_schema = 0ull;
    size_t decoder_bytes = 0u;
    int rc;
    if (out) *out = NULL;
    if (!out || !data || !count || !plan_get_text(&cursor, domain))
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled model-plan header is malformed");
    if (strcmp(domain, "yvex.compiled-model-plan.v2") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V2;
    else if (strcmp(domain, "yvex.compiled-model-plan.v3") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V3;
    else if (strcmp(domain, "yvex.compiled-model-plan.v4") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V4;
    else if (strcmp(domain, "yvex.compiled-model-plan.v5") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V5;
    else if (strcmp(domain, "yvex.compiled-model-plan.v6") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V6;
    else if (strcmp(domain, "yvex.compiled-model-plan.v7") == 0)
        expected_schema = MODEL_PLAN_SCHEMA_V7;
    if (!expected_schema || !plan_get_u64(&cursor, &schema) ||
        schema != expected_schema)
        return model_plan_refuse(err, YVEX_ERR_FORMAT,
                                 "compiled model-plan header is malformed");
    plan = (yvex_compiled_model_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return model_plan_refuse(err, YVEX_ERR_NOMEM,
                                 "compiled model-plan allocation failed");
    if (!plan_get_text(&cursor, plan->operator_graph_identity) ||
        !yvex_sha256_hex_valid(plan->operator_graph_identity))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled operator graph identity is malformed");
    else if (!plan_get_u64(&cursor, &target_present) || target_present > 1ull)
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled target-plan presence is malformed");
    else rc = YVEX_OK;
    if (rc == YVEX_OK && target_present)
        rc = moe_plan_read(&cursor, &plan->moe, err);
    if (rc == YVEX_OK && target_present)
        rc = transformer_plan_read(&cursor, &plan->transformer, err);
    if (rc == YVEX_OK && target_present && schema < MODEL_PLAN_SCHEMA_V4)
        rc = output_head_read(&cursor, &plan->output_head, 1, err);
    if (rc == YVEX_OK &&
        (!plan_get_u64(&cursor, &draft_present) || draft_present > 1ull))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled draft-plan presence is malformed");
    if (rc == YVEX_OK && draft_present)
        rc = moe_plan_read(&cursor, &plan->draft_moe, err);
    if (rc == YVEX_OK && draft_present)
        rc = transformer_plan_read(&cursor, &plan->draft_transformer, err);
    if (rc == YVEX_OK && schema >= MODEL_PLAN_SCHEMA_V3 &&
        (!plan_get_u64(&cursor, &decoder_present) || decoder_present > 1ull))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled decoder-plan presence is malformed");
    if (rc == YVEX_OK && decoder_present)
        rc = yvex_decoder_plan_decode(
            &plan->decoder, cursor.data + cursor.offset,
            cursor.count - cursor.offset, &decoder_bytes, err);
    if (rc == YVEX_OK && decoder_present)
        cursor.offset += decoder_bytes;
    if (rc == YVEX_OK && schema >= MODEL_PLAN_SCHEMA_V4 &&
        (!plan_get_u64(&cursor, &output_present) || output_present > 1ull))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled output-head presence is malformed");
    if (rc == YVEX_OK && output_present)
        rc = output_head_read(&cursor, &plan->output_head, 0, err);
    plan->schema = (unsigned int)schema;
    if (rc == YVEX_OK && schema == MODEL_PLAN_SCHEMA_V5) {
        unsigned long long length;
        if (!decoder_present || !plan_get_u64(&cursor, &length) || length > cursor.count - cursor.offset)
            rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "compiled tensor program extent is malformed");
        else {
            rc = yvex_program_tensor_decode(&plan->dense_ffn, cursor.data + cursor.offset, (size_t)length, err);
            if (rc == YVEX_OK) cursor.offset += (size_t)length;
        }
    } else if (rc == YVEX_OK && decoder_present && schema < MODEL_PLAN_SCHEMA_V5)
        rc = legacy_decoder_ffn_import(&plan->dense_ffn, plan->decoder, err);
    if (rc == YVEX_OK && schema >= MODEL_PLAN_SCHEMA_V6) {
        unsigned long long length;
        if (!plan_get_u64(&cursor, &length) || length > cursor.count - cursor.offset)
            rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "compiled forward program extent is malformed");
        else {
            rc = yvex_program_physical_decode(&plan->forward, cursor.data + cursor.offset, (size_t)length, err);
            if (rc == YVEX_OK) cursor.offset += (size_t)length;
        }
    }
    if (rc == YVEX_OK && schema == MODEL_PLAN_SCHEMA_V7) {
        unsigned long long length = 0u;
        if (!plan_get_u64(&cursor, &length) || (length != 0u) != (output_present != 0u) ||
            length > cursor.count - cursor.offset)
            rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "compiled output program extent is malformed");
        else if (length) {
            rc = yvex_program_physical_decode(&plan->output, cursor.data + cursor.offset, (size_t)length, err);
            if (rc == YVEX_OK) cursor.offset += (size_t)length;
        }
    }
    if (rc == YVEX_OK && !compiled_output_program_valid(plan))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "output program differs from its producer signature/lineage");
    if (rc == YVEX_OK &&
        ((target_present && decoder_present) ||
         (!target_present && draft_present) ||
         !compiled_output_matches_producer(plan) || !compiled_ffn_signature_valid(plan)))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled execution producers are inconsistent");
    if (rc == YVEX_OK && cursor.offset != cursor.count)
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT,
                               "compiled model-plan has trailing bytes");
    if (rc == YVEX_OK) *out = plan;
    else yvex_compiled_model_plan_close(&plan);
    return rc;
}

static int decoder_output_admitted(
    const yvex_runtime_logits_plan_summary *output,
    const yvex_decoder_plan_summary *decoder,
    const yvex_compiled_model_plan_admission *admission)
{
    int present = output && output->schema_version;
    if (!decoder || !admission || !admission->capabilities ||
        admission->capabilities->output_head_binding_ready !=
            admission->capabilities->output_head_projection_ready ||
        present != admission->capabilities->output_head_projection_ready)
        return 0;
    if (!present)
        return !admission->output_head_plan_identity ||
               !admission->output_head_plan_identity[0];
    return admission->output_head_plan_identity &&
           admission->artifact_identity && admission->materialization_identity &&
           admission->runtime_descriptor_identity &&
           output->producer_kind == YVEX_EXECUTION_PLAN_DECODER &&
           output->family_adapter_id == admission->family_adapter_id &&
           output->family_adapter_version == admission->family_adapter_version &&
           output->hidden_width == decoder->hidden_width &&
           output->vocabulary_size == decoder->vocabulary_size &&
           output->output_head_tensor_id < admission->tensor_count &&
           strcmp(output->decoder_plan_identity,
                  decoder->decoder_plan_identity) == 0 &&
           strcmp(output->artifact_identity,
                  admission->artifact_identity) == 0 &&
           strcmp(output->materialization_identity,
                  admission->materialization_identity) == 0 &&
           strcmp(output->runtime_descriptor_identity,
                  admission->runtime_descriptor_identity) == 0 &&
           strcmp(output->output_head_plan_identity,
                  admission->output_head_plan_identity) == 0;
}

int yvex_compiled_model_plan_normalize(yvex_compiled_model_plan *plan,
    const yvex_attention_layer_plan *attention, size_t attention_count,
    const yvex_physical_execution_ir *parameters, yvex_error *err)
{
    if (!plan) return model_plan_refuse(err, YVEX_ERR_INVALID_ARG, "compiled import owner required");
    int rc = YVEX_OK;
    if (plan->transformer && !plan->final)
        rc = yvex_transformer_final_program_import(&plan->final, plan->transformer, parameters, err);
    if (rc == YVEX_OK && plan->draft_transformer && !plan->draft_final)
        rc = yvex_transformer_final_program_import(&plan->draft_final, plan->draft_transformer, parameters, err);
    if (rc != YVEX_OK) return rc;
    if (plan->output_head.schema_version) {
        const yvex_decoder_plan_summary *decoder = yvex_decoder_plan_summary_get(plan->decoder);
        const yvex_transformer_plan_summary *transformer = yvex_transformer_plan_summary_get(plan->transformer);
        unsigned long long rows = decoder ? decoder->maximum_context :
            transformer ? transformer->maximum_context : 0u;
        if (!plan->output && plan->schema == MODEL_PLAN_SCHEMA_V7)
            return model_plan_refuse(err, YVEX_ERR_FORMAT, "native binding lacks its output program");
        if (!plan->output)
            rc = yvex_output_head_program_import(&plan->output, &plan->output_head, rows, parameters, err);
        if (rc == YVEX_OK) rc = yvex_output_head_program_validate(plan->output, &plan->output_head, parameters, err);
        if (rc == YVEX_OK && !compiled_output_program_valid(plan))
            rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "output import differs from the authenticated producer");
    }
    if (!plan->decoder || rc != YVEX_OK) return rc;
    /* The enclosing binding has authenticated both old topology and physical
     * parameters. Retained old bytes serve serialization identity only. */
    rc = plan->forward ? YVEX_OK : yvex_decoder_plan_normalize_program(&plan->forward, plan->decoder,
        attention, attention_count, parameters, err);
    if (rc == YVEX_OK && !compiled_forward_signature_valid(plan))
        rc = model_plan_refuse(err, YVEX_ERR_FORMAT, "imported forward contradicts the retained producer view");
    if (rc == YVEX_OK) rc = yvex_program_physical_parameters_validate(plan->forward, parameters, err);
    return rc;
}

int yvex_compiled_model_plan_admit(
    const yvex_compiled_model_plan *plans,
    const yvex_compiled_model_plan_admission *admission)
{
    yvex_compiled_context_envelope context;
    yvex_error err;
    const yvex_moe_plan_summary *moe;
    const yvex_moe_plan_summary *draft_moe;
    const yvex_transformer_plan_summary *transformer;
    const yvex_transformer_plan_summary *draft_transformer;
    const yvex_decoder_plan_summary *decoder;
    const yvex_runtime_logits_plan_summary *output;
    if (!plans || !admission || !admission->capabilities ||
        !yvex_runtime_capabilities_contract_valid(admission->capabilities))
        return 0;
    moe = yvex_moe_plan_summary_get(plans->moe);
    draft_moe = yvex_moe_plan_summary_get(plans->draft_moe);
    transformer = yvex_transformer_plan_summary_get(plans->transformer);
    draft_transformer =
        yvex_transformer_plan_summary_get(plans->draft_transformer);
    decoder = yvex_decoder_plan_summary_get(plans->decoder);
    output = &plans->output_head;
    if (decoder) {
        return yvex_compiled_model_plan_context_envelope(
                   plans, admission->model_execution_identity,
                   admission->semantic_maximum_context, &context, &err) == YVEX_OK &&
               !moe && !draft_moe && !transformer && !draft_transformer &&
               !admission->capabilities->moe_plan_ready &&
               !admission->capabilities->transformer_ready &&
               !admission->capabilities->logits_ready &&
               decoder->family_adapter_id == admission->family_adapter_id &&
               decoder->family_adapter_version == admission->family_adapter_version &&
               decoder->layer_count == admission->decoder_layer_count &&
               decoder->attention_layer_count == admission->layer_count &&
               decoder->recurrent_layer_count == admission->recurrent_layer_count &&
               decoder->maximum_context == admission->semantic_maximum_context &&
               strcmp(decoder->model_execution_identity,
                      admission->model_execution_identity) == 0 &&
               admission->decoder_plan_identity &&
               strcmp(decoder->decoder_plan_identity,
                      admission->decoder_plan_identity) == 0 &&
               decoder_output_admitted(output, decoder, admission);
    }
    if (!moe || !transformer || !output->schema_version)
        return !moe && !transformer && !draft_moe && !draft_transformer &&
               !output->schema_version &&
               !admission->capabilities->moe_plan_ready &&
               !admission->capabilities->transformer_ready &&
               !admission->capabilities->logits_ready;
    if (yvex_compiled_model_plan_context_envelope(
            plans, admission->model_execution_identity,
            admission->semantic_maximum_context, &context, &err) != YVEX_OK ||
        !admission->capabilities->moe_plan_ready ||
        !admission->capabilities->transformer_ready ||
        !admission->capabilities->logits_ready ||
        moe->family_adapter_id != admission->family_adapter_id ||
        moe->family_adapter_version != admission->family_adapter_version ||
        moe->layer_count != admission->layer_count ||
        strcmp(moe->artifact_identity, admission->artifact_identity) != 0 ||
        strcmp(moe->materialization_identity,
               admission->materialization_identity) != 0 ||
        strcmp(moe->runtime_descriptor_identity,
               admission->runtime_descriptor_identity) != 0 ||
        strcmp(moe->attention_plan_identity,
               admission->attention_plan_identity) != 0 ||
        strcmp(moe->moe_plan_identity, admission->moe_plan_identity) != 0 ||
        strcmp(transformer->moe_plan_identity, moe->moe_plan_identity) != 0 ||
        strcmp(transformer->attention_plan_identity,
               moe->attention_plan_identity) != 0 ||
        strcmp(transformer->transformer_plan_identity,
               admission->transformer_plan_identity) != 0 ||
        output->output_head_tensor_id >= admission->tensor_count ||
        output->producer_kind != YVEX_EXECUTION_PLAN_TRANSFORMER ||
        strcmp(output->transformer_plan_identity,
               transformer->transformer_plan_identity) != 0 ||
        strcmp(output->output_head_plan_identity,
               admission->output_head_plan_identity) != 0)
        return 0;
    if (!admission->draft_layer_count)
        return !draft_moe && !draft_transformer;
    return draft_moe && draft_transformer &&
           draft_moe->layer_count == admission->draft_layer_count &&
           strcmp(draft_moe->attention_plan_identity,
                  admission->draft_attention_plan_identity) == 0 &&
           strcmp(draft_moe->moe_plan_identity,
                  admission->draft_moe_plan_identity) == 0 &&
           strcmp(draft_transformer->moe_plan_identity,
                  draft_moe->moe_plan_identity) == 0 &&
           strcmp(draft_transformer->attention_plan_identity,
                  draft_moe->attention_plan_identity) == 0 &&
           strcmp(draft_transformer->transformer_plan_identity,
                  admission->draft_transformer_plan_identity) == 0;
}

int yvex_compiled_model_plan_context_envelope(
    const yvex_compiled_model_plan *plan,
    const char *model_execution_identity,
    unsigned long long semantic_maximum_context,
    yvex_compiled_context_envelope *envelope, yvex_error *err)
{
    const yvex_transformer_plan_summary *target =
        yvex_transformer_plan_summary_get(
            yvex_compiled_model_plan_transformer(plan, 0));
    const yvex_transformer_plan_summary *draft =
        yvex_transformer_plan_summary_get(
            yvex_compiled_model_plan_transformer(plan, 1));
    const yvex_decoder_plan_summary *decoder =
        yvex_decoder_plan_summary_get(yvex_compiled_model_plan_decoder(plan));
    unsigned long long target_maximum;
    if (envelope) memset(envelope, 0, sizeof(*envelope));
    if (!envelope || !yvex_sha256_hex_valid(model_execution_identity) ||
        !semantic_maximum_context || (!!target == !!decoder) ||
        (target &&
         (target->maximum_context != semantic_maximum_context ||
          !yvex_sha256_hex_valid(target->transformer_plan_identity))) ||
        (decoder &&
         (decoder->maximum_context != semantic_maximum_context ||
          !yvex_sha256_hex_valid(decoder->decoder_plan_identity) || draft)) ||
        (draft && (draft->maximum_context != semantic_maximum_context ||
                   !yvex_sha256_hex_valid(draft->transformer_plan_identity))))
        return model_plan_refuse(
            err, YVEX_ERR_FORMAT,
            "compiled context envelope does not match semantic model capability");
    target_maximum = target ? target->maximum_context : decoder->maximum_context;
    envelope->schema_version = YVEX_COMPILED_CONTEXT_ENVELOPE_SCHEMA_V2;
    envelope->target_kind = target ? YVEX_EXECUTION_PLAN_TRANSFORMER
                                   : YVEX_EXECUTION_PLAN_DECODER;
    envelope->semantic_maximum_context = semantic_maximum_context;
    envelope->target_maximum_context = target_maximum;
    envelope->draft_available = draft != NULL;
    envelope->draft_maximum_context = draft ? draft->maximum_context : 0ull;
    yvex_core_text_copy(envelope->model_execution_identity,
                        sizeof(envelope->model_execution_identity),
                        model_execution_identity);
    if (target)
        yvex_core_text_copy(envelope->target_transformer_identity,
                            sizeof(envelope->target_transformer_identity),
                            target->transformer_plan_identity);
    else
        yvex_core_text_copy(envelope->target_decoder_identity,
                            sizeof(envelope->target_decoder_identity),
                            decoder->decoder_plan_identity);
    if (draft)
        yvex_core_text_copy(envelope->draft_transformer_identity,
                            sizeof(envelope->draft_transformer_identity),
                            draft->transformer_plan_identity);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_compiled_context_envelope_admit(
    const yvex_compiled_context_envelope *envelope,
    unsigned long long requested_context, int require_draft, yvex_error *err)
{
    unsigned long long maximum;
    int legacy, transformer_target, decoder_target;
    if (!envelope) return model_plan_refuse(
        err, YVEX_ERR_INVALID_ARG,
        "compiled context admission requires one bounded runtime request");
    legacy = envelope->schema_version ==
             YVEX_COMPILED_CONTEXT_ENVELOPE_SCHEMA_V1;
    transformer_target =
        (legacy && envelope->target_kind == YVEX_EXECUTION_PLAN_UNKNOWN) ||
        (!legacy &&
         envelope->target_kind == YVEX_EXECUTION_PLAN_TRANSFORMER);
    decoder_target = !legacy &&
        envelope->target_kind == YVEX_EXECUTION_PLAN_DECODER;
    if ((!legacy && envelope->schema_version !=
                        YVEX_COMPILED_CONTEXT_ENVELOPE_SCHEMA_V2) ||
        (!transformer_target && !decoder_target) ||
        !envelope->semantic_maximum_context || !envelope->target_maximum_context ||
        envelope->semantic_maximum_context != envelope->target_maximum_context ||
        !yvex_sha256_hex_valid(envelope->model_execution_identity) ||
        (transformer_target &&
         (!yvex_sha256_hex_valid(envelope->target_transformer_identity) ||
          envelope->target_decoder_identity[0])) ||
        (decoder_target &&
         (!yvex_sha256_hex_valid(envelope->target_decoder_identity) ||
          envelope->target_transformer_identity[0] ||
          envelope->draft_available || envelope->draft_maximum_context ||
          envelope->draft_transformer_identity[0])) ||
        (envelope->draft_available &&
         (!transformer_target ||
          envelope->draft_maximum_context != envelope->semantic_maximum_context ||
          !yvex_sha256_hex_valid(envelope->draft_transformer_identity))) ||
        (!envelope->draft_available &&
         (envelope->draft_maximum_context || envelope->draft_transformer_identity[0])) ||
        (require_draft != 0 && require_draft != 1) || !requested_context)
        return model_plan_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "compiled context admission requires one bounded runtime request");
    if (require_draft && !envelope->draft_available)
        return model_plan_refuse(
            err, YVEX_ERR_UNSUPPORTED,
            "compiled context envelope does not admit draft execution");
    maximum = require_draft ? envelope->draft_maximum_context
                            : envelope->target_maximum_context;
    if (requested_context > maximum)
        return model_plan_refuse(
            err, YVEX_ERR_BOUNDS,
            "requested context exceeds the compiled semantic maximum");
    yvex_error_clear(err);
    return YVEX_OK;
}

const yvex_moe_plan *yvex_compiled_model_plan_moe(
    const yvex_compiled_model_plan *plan, int draft)
{
    return plan ? (draft ? plan->draft_moe : plan->moe) : NULL;
}

const yvex_transformer_plan *yvex_compiled_model_plan_transformer(
    const yvex_compiled_model_plan *plan, int draft)
{
    return plan ? (draft ? plan->draft_transformer : plan->transformer) : NULL;
}

const yvex_decoder_plan *yvex_compiled_model_plan_decoder(
    const yvex_compiled_model_plan *plan)
{
    return plan ? plan->decoder : NULL;
}

const yvex_runtime_logits_plan_summary *yvex_compiled_model_plan_output_head(
    const yvex_compiled_model_plan *plan)
{
    return plan && plan->output_head.schema_version ? &plan->output_head : NULL;
}

const yvex_program_tensor_plan *yvex_compiled_model_plan_dense_ffn(const yvex_compiled_model_plan *plan)
{
    return plan ? plan->dense_ffn : NULL;
}

const yvex_program_physical *yvex_compiled_model_plan_forward(const yvex_compiled_model_plan *plan)
{
    return plan ? plan->forward : NULL;
}

const yvex_program_physical *yvex_compiled_model_plan_output(const yvex_compiled_model_plan *plan)
{
    return plan ? plan->output : NULL;
}

const yvex_program_physical *yvex_compiled_model_plan_final(const yvex_compiled_model_plan *plan, int draft)
{
    return plan ? (draft ? plan->draft_final : plan->final) : NULL;
}

const char *yvex_compiled_model_plan_operator_graph_identity(
    const yvex_compiled_model_plan *plan)
{
    return plan && yvex_sha256_hex_valid(plan->operator_graph_identity)
               ? plan->operator_graph_identity : NULL;
}
