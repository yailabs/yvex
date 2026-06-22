/*
 * YVEX - Tensor role classifier
 *
 * File: src/model/role.c
 * Layer: model implementation
 *
 * Purpose:
 *   Implements a small tensor-name classifier for known LLM tensor naming
 *   patterns. Unknown roles are allowed and are not parse failures.
 *
 * Implements:
 *   - yvex_tensor_role_name
 *   - yvex_tensor_role_classify
 *
 * Invariants:
 *   - classifier does not allocate
 *   - unknown names map to YVEX_TENSOR_ROLE_UNKNOWN
 *   - classifier does not imply architecture validation
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tensor_table
 */
#include <yvex/tensor.h>

#include <string.h>

const char *yvex_tensor_role_name(yvex_tensor_role role)
{
    switch (role) {
    case YVEX_TENSOR_ROLE_UNKNOWN: return "unknown";
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING: return "token_embedding";
    case YVEX_TENSOR_ROLE_OUTPUT_NORM: return "output_norm";
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD: return "output_head";
    case YVEX_TENSOR_ROLE_ATTENTION_NORM: return "attention_norm";
    case YVEX_TENSOR_ROLE_ATTENTION_Q: return "attention_q";
    case YVEX_TENSOR_ROLE_ATTENTION_K: return "attention_k";
    case YVEX_TENSOR_ROLE_ATTENTION_V: return "attention_v";
    case YVEX_TENSOR_ROLE_ATTENTION_OUT: return "attention_out";
    case YVEX_TENSOR_ROLE_FFN_NORM: return "ffn_norm";
    case YVEX_TENSOR_ROLE_FFN_GATE: return "ffn_gate";
    case YVEX_TENSOR_ROLE_FFN_UP: return "ffn_up";
    case YVEX_TENSOR_ROLE_FFN_DOWN: return "ffn_down";
    case YVEX_TENSOR_ROLE_MOE_ROUTER: return "moe_router";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_GATE: return "moe_expert_gate";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_UP: return "moe_expert_up";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN: return "moe_expert_down";
    }
    return "unknown";
}

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) {
        return 0;
    }

    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return 0;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int contains(const char *text, const char *needle)
{
    return text && needle && strstr(text, needle) != NULL;
}

yvex_tensor_role yvex_tensor_role_classify(const char *architecture,
                                           const char *tensor_name,
                                           unsigned int rank,
                                           const unsigned long long *dims,
                                           yvex_dtype dtype)
{
    (void)architecture;
    (void)rank;
    (void)dims;
    (void)dtype;

    if (!tensor_name) {
        return YVEX_TENSOR_ROLE_UNKNOWN;
    }

    if (strcmp(tensor_name, "token_embd.weight") == 0 ||
        strcmp(tensor_name, "model.embed_tokens.weight") == 0 ||
        strcmp(tensor_name, "tok_embeddings.weight") == 0) {
        return YVEX_TENSOR_ROLE_TOKEN_EMBEDDING;
    }
    if (strcmp(tensor_name, "output_norm.weight") == 0 ||
        strcmp(tensor_name, "model.norm.weight") == 0 ||
        strcmp(tensor_name, "norm.weight") == 0) {
        return YVEX_TENSOR_ROLE_OUTPUT_NORM;
    }
    if (strcmp(tensor_name, "output.weight") == 0 ||
        strcmp(tensor_name, "lm_head.weight") == 0) {
        return YVEX_TENSOR_ROLE_OUTPUT_HEAD;
    }
    if (ends_with(tensor_name, ".attn_norm.weight") ||
        ends_with(tensor_name, ".input_layernorm.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_NORM;
    }
    if (ends_with(tensor_name, ".attn_q.weight") ||
        ends_with(tensor_name, ".self_attn.q_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_Q;
    }
    if (ends_with(tensor_name, ".attn_k.weight") ||
        ends_with(tensor_name, ".self_attn.k_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_K;
    }
    if (ends_with(tensor_name, ".attn_v.weight") ||
        ends_with(tensor_name, ".self_attn.v_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_V;
    }
    if (ends_with(tensor_name, ".attn_output.weight") ||
        ends_with(tensor_name, ".self_attn.o_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_OUT;
    }
    if (ends_with(tensor_name, ".ffn_norm.weight") ||
        ends_with(tensor_name, ".post_attention_layernorm.weight")) {
        return YVEX_TENSOR_ROLE_FFN_NORM;
    }
    if (ends_with(tensor_name, ".ffn_gate.weight") ||
        ends_with(tensor_name, ".mlp.gate_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_GATE;
    }
    if (ends_with(tensor_name, ".ffn_up.weight") ||
        ends_with(tensor_name, ".mlp.up_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_UP;
    }
    if (ends_with(tensor_name, ".ffn_down.weight") ||
        ends_with(tensor_name, ".mlp.down_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_DOWN;
    }
    if (ends_with(tensor_name, ".ffn_gate_inp.weight") ||
        ends_with(tensor_name, ".mlp.gate.weight")) {
        return YVEX_TENSOR_ROLE_MOE_ROUTER;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".gate.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".up.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".down.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
    }

    return YVEX_TENSOR_ROLE_UNKNOWN;
}
