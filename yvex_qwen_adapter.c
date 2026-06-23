/*
 * YVEX - Qwen weight mapping adapter
 */
#include "yvex_qwen_adapter.h"

#include <stdio.h>
#include <string.h>

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;
    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    return suffix_len <= text_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int extract_layer(const char *name, unsigned int *layer)
{
    return name && sscanf(name, "model.layers.%u.", layer) == 1;
}

static int set_target(char *target, size_t cap, const char *fmt, unsigned int layer)
{
    int n = snprintf(target, cap, fmt, layer);
    return n > 0 && (size_t)n < cap;
}

int yvex_qwen_adapter_map_name(const char *native_name,
                               char *target,
                               size_t target_cap,
                               yvex_tensor_role *role,
                               yvex_weight_mapping_issue_kind *issue)
{
    unsigned int layer = 0;

    if (role) *role = YVEX_TENSOR_ROLE_UNKNOWN;
    if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    if (!native_name || !target || target_cap == 0) {
        if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
        return 0;
    }

    if (strcmp(native_name, "model.embed_tokens.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_TOKEN_EMBEDDING;
        snprintf(target, target_cap, "%s", "token_embd.weight");
        return 1;
    }
    if (strcmp(native_name, "model.norm.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_OUTPUT_NORM;
        snprintf(target, target_cap, "%s", "output_norm.weight");
        return 1;
    }
    if (strcmp(native_name, "lm_head.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_OUTPUT_HEAD;
        snprintf(target, target_cap, "%s", "output.weight");
        return 1;
    }

    if (!extract_layer(native_name, &layer)) {
        if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
        return 0;
    }

    if (ends_with(native_name, ".self_attn.q_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_Q;
        return set_target(target, target_cap, "blk.%u.attn_q.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.k_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_K;
        return set_target(target, target_cap, "blk.%u.attn_k.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.v_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_V;
        return set_target(target, target_cap, "blk.%u.attn_v.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.o_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_OUT;
        return set_target(target, target_cap, "blk.%u.attn_output.weight", layer);
    }
    if (ends_with(native_name, ".input_layernorm.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        return set_target(target, target_cap, "blk.%u.attn_norm.weight", layer);
    }
    if (ends_with(native_name, ".post_attention_layernorm.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_NORM;
        return set_target(target, target_cap, "blk.%u.ffn_norm.weight", layer);
    }
    if (ends_with(native_name, ".mlp.gate_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_GATE;
        return set_target(target, target_cap, "blk.%u.ffn_gate.weight", layer);
    }
    if (ends_with(native_name, ".mlp.up_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_UP;
        return set_target(target, target_cap, "blk.%u.ffn_up.weight", layer);
    }
    if (ends_with(native_name, ".mlp.down_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_DOWN;
        return set_target(target, target_cap, "blk.%u.ffn_down.weight", layer);
    }

    if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
    return 0;
}
