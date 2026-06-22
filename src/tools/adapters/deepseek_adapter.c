/*
 * YVEX - DeepSeek tensor-name adapter skeleton
 *
 * File: src/tools/adapters/deepseek_adapter.c
 * Layer: tool-plane implementation
 */
#include "deepseek_adapter.h"

#include <stdio.h>
#include <string.h>

static int ds_set(char *target, size_t target_cap,
                  yvex_tensor_role *role,
                  yvex_weight_mapping_issue_kind *issue,
                  yvex_tensor_role value,
                  const char *name)
{
    if (!target || target_cap == 0 || !role || !issue || !name) {
        return 0;
    }
    if (snprintf(target, target_cap, "%s", name) >= (int)target_cap) {
        *role = YVEX_TENSOR_ROLE_UNKNOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME;
        return 0;
    }
    *role = value;
    *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    return 1;
}

static int ds_starts_with(const char *text, const char *prefix)
{
    size_t len;

    if (!text || !prefix) return 0;
    len = strlen(prefix);
    return strncmp(text, prefix, len) == 0;
}

static int ds_ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    return suffix_len <= text_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int ds_layer_suffix(const char *native_name,
                           const char *suffix,
                           unsigned int *layer_out)
{
    unsigned int layer;
    int consumed;

    if (sscanf(native_name, "model.layers.%u.%n", &layer, &consumed) != 1) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    return 1;
}

static int ds_plain_layer_suffix(const char *native_name,
                                 const char *suffix,
                                 unsigned int *layer_out)
{
    unsigned int layer;
    int consumed;

    if (sscanf(native_name, "layers.%u.%n", &layer, &consumed) != 1) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    return 1;
}

static int ds_expert_suffix(const char *native_name,
                            const char *suffix,
                            unsigned int *layer_out,
                            unsigned int *expert_out)
{
    unsigned int layer;
    unsigned int expert;
    int consumed;

    if (sscanf(native_name, "model.layers.%u.mlp.experts.%u.%n",
               &layer, &expert, &consumed) != 2) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    *expert_out = expert;
    return 1;
}

static int ds_plain_expert_suffix(const char *native_name,
                                  const char *suffix,
                                  unsigned int *layer_out,
                                  unsigned int *expert_out)
{
    unsigned int layer;
    unsigned int expert;
    int consumed;

    if (sscanf(native_name, "layers.%u.ffn.experts.%u.%n",
               &layer, &expert, &consumed) != 2) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    *expert_out = expert;
    return 1;
}

static int ds_template_style(const char *native_name,
                             char *target,
                             size_t target_cap,
                             yvex_tensor_role *role,
                             yvex_weight_mapping_issue_kind *issue)
{
    if (strcmp(native_name, "token_embd.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "token_embd.weight");
    }
    if (strcmp(native_name, "output_norm.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_NORM, "output_norm.weight");
    }
    if (strcmp(native_name, "output.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_HEAD, "output.weight");
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_q.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_Q, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_k.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_K, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_v.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_V, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_output.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_OUT, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_norm.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_NORM, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_norm.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_NORM, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_gate.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_GATE, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_up.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_UP, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_down.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_DOWN, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_gate_inp.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_ROUTER, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".gate.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".up.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_UP, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".down.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, native_name);
    }
    return 0;
}

int yvex_deepseek_adapter_map_name(const char *native_name,
                                   char *target,
                                   size_t target_cap,
                                   yvex_tensor_role *role,
                                   yvex_weight_mapping_issue_kind *issue)
{
    unsigned int layer;
    unsigned int expert;

    if (!native_name || !target || target_cap == 0 || !role || !issue) {
        return 0;
    }
    target[0] = '\0';
    *role = YVEX_TENSOR_ROLE_UNKNOWN;
    *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;

    if (ds_template_style(native_name, target, target_cap, role, issue)) {
        return 1;
    }
    if (strcmp(native_name, "embed.weight") == 0 ||
        strcmp(native_name, "model.embed_tokens.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "token_embd.weight");
    }
    if (strcmp(native_name, "norm.weight") == 0 ||
        strcmp(native_name, "model.norm.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_NORM, "output_norm.weight");
    }
    if (strcmp(native_name, "lm_head.weight") == 0 ||
        strcmp(native_name, "output.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_HEAD, "output.weight");
    }
    if (ds_layer_suffix(native_name, "self_attn.q_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_q.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_Q;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.k_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_k.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_K;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.v_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_v.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_V;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.o_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_output.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_OUT;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "input_layernorm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "attn_norm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "post_attention_layernorm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "ffn_norm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.gate_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.up_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_up.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.down_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_down.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.gate.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate_inp.weight", layer);
        *role = YVEX_TENSOR_ROLE_MOE_ROUTER;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "ffn.gate.weight", &layer) ||
        ds_plain_layer_suffix(native_name, "ffn.gate.bias", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate_inp.weight", layer);
        *role = YVEX_TENSOR_ROLE_MOE_ROUTER;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "gate_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.gate.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "up_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.up.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "down_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.down.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w1.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.gate.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w2.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.down.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w3.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.up.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }

    return 0;
}
