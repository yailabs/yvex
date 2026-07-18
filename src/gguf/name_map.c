/*
 * name_map.c - emitted GGUF tensor name plan facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   runtime-role to emitted GGUF tensor-name plan boundary and missing-name
 *   blockers.
 *
 * Does not own:
 *   source native role mapping, layout/range planning, writer bytes,
 *   materialization, graph binding, or generation.
 *
 * Invariants:
 *   pinned standard names match llama.cpp e920c523; MTP names are explicitly
 *   versioned YVEX extensions because that converter discards mtp.*.
 *
 * Boundary:
 *   name-map facts do not imply byte layout or artifact emission.
 */
#include "map.h"

#include <stdio.h>
#include <string.h>

static const char *gguf_standard_role_pattern(yvex_tensor_role role)
{
    switch (role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING: return "token_embd.weight";
    case YVEX_TENSOR_ROLE_OUTPUT_NORM: return "output_norm.weight";
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD: return "output.weight";
    case YVEX_TENSOR_ROLE_HC_HEAD_FUNCTION: return "output_hc_fn.weight";
    case YVEX_TENSOR_ROLE_HC_HEAD_BASE: return "output_hc_base.weight";
    case YVEX_TENSOR_ROLE_HC_HEAD_SCALE: return "output_hc_scale.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_NORM: return "blk.%llu.attn_norm.weight";
    case YVEX_TENSOR_ROLE_FFN_NORM: return "blk.%llu.ffn_norm.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_SINKS: return "blk.%llu.attn_sinks.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_Q_A: return "blk.%llu.attn_q_a.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_Q_B: return "blk.%llu.attn_q_b.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM: return "blk.%llu.attn_q_a_norm.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_KV: return "blk.%llu.attn_kv.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_KV_NORM: return "blk.%llu.attn_kv_a_norm.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_OUT_A: return "blk.%llu.attn_output_a.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_OUT_B: return "blk.%llu.attn_output_b.weight";
    case YVEX_TENSOR_ROLE_HC_ATTENTION_FUNCTION: return "blk.%llu.hc_attn_fn.weight";
    case YVEX_TENSOR_ROLE_HC_ATTENTION_BASE: return "blk.%llu.hc_attn_base.weight";
    case YVEX_TENSOR_ROLE_HC_ATTENTION_SCALE: return "blk.%llu.hc_attn_scale.weight";
    case YVEX_TENSOR_ROLE_HC_FFN_FUNCTION: return "blk.%llu.hc_ffn_fn.weight";
    case YVEX_TENSOR_ROLE_HC_FFN_BASE: return "blk.%llu.hc_ffn_base.weight";
    case YVEX_TENSOR_ROLE_HC_FFN_SCALE: return "blk.%llu.hc_ffn_scale.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV: return "blk.%llu.attn_compressor_kv.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE: return "blk.%llu.attn_compressor_gate.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE: return "blk.%llu.attn_compressor_ape.weight";
    case YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM: return "blk.%llu.attn_compressor_norm.weight";
    case YVEX_TENSOR_ROLE_INDEXER_PROJECTION: return "blk.%llu.indexer.proj.weight";
    case YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B: return "blk.%llu.indexer.attn_q_b.weight";
    case YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV: return "blk.%llu.indexer_compressor_kv.weight";
    case YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE: return "blk.%llu.indexer_compressor_gate.weight";
    case YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE: return "blk.%llu.indexer_compressor_ape.weight";
    case YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM: return "blk.%llu.indexer_compressor_norm.weight";
    case YVEX_TENSOR_ROLE_MOE_ROUTER: return "blk.%llu.ffn_gate_inp.weight";
    case YVEX_TENSOR_ROLE_MOE_ROUTER_BIAS: return "blk.%llu.exp_probs_b.bias";
    case YVEX_TENSOR_ROLE_MOE_ROUTER_TABLE: return "blk.%llu.ffn_gate_tid2eid.weight";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_GATE: return "blk.%llu.ffn_gate_exps.weight";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN: return "blk.%llu.ffn_down_exps.weight";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_UP: return "blk.%llu.ffn_up_exps.weight";
    case YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_GATE: return "blk.%llu.ffn_gate_shexp.weight";
    case YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_DOWN: return "blk.%llu.ffn_down_shexp.weight";
    case YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_UP: return "blk.%llu.ffn_up_shexp.weight";
    default: return NULL;
    }
}

/* Resolves one typed role without inspecting or classifying a source name. */
int yvex_gguf_name_map_resolve(yvex_tensor_role role,
                               int mtp_extension,
                               unsigned long long layer_index,
                               unsigned long long predictor_index,
                               char *out,
                               size_t out_cap,
                               yvex_gguf_name_provenance *provenance,
                               const char **reason)
{
    const char *pattern;
    int written;

    if (!out || out_cap == 0u || !provenance || role <= YVEX_TENSOR_ROLE_UNKNOWN ||
        role >= YVEX_TENSOR_ROLE_COUNT) {
        if (reason) *reason = "invalid typed role or output buffer";
        return 0;
    }
    if (mtp_extension) {
        written = snprintf(out, out_cap, "yvex.mtp.v1.%llu.%s.weight",
                           predictor_index, yvex_tensor_role_name(role));
        *provenance = YVEX_GGUF_NAME_YVEX_EXTENSION;
    } else {
        pattern = gguf_standard_role_pattern(role);
        if (!pattern) {
            if (reason) *reason = "role has no pinned DeepSeek-V4 GGUF name";
            return 0;
        }
        if (strstr(pattern, "%llu")) written = snprintf(out, out_cap, pattern, layer_index);
        else written = snprintf(out, out_cap, "%s", pattern);
        *provenance = YVEX_GGUF_NAME_PINNED_STANDARD;
    }
    if (written < 0 || (size_t)written >= out_cap) {
        if (reason) *reason = "emitted GGUF name exceeds mapping bounds";
        return 0;
    }
    if (reason) *reason = "admitted canonical logical name";
    return 1;
}

const char *yvex_gguf_name_provenance_name(
    yvex_gguf_name_provenance provenance)
{
    static const char *names[] = {
        "pinned-standard", "semantically-reused-standard",
        "yvex-extension-v1"
    };
    return provenance <= YVEX_GGUF_NAME_YVEX_EXTENSION
               ? names[provenance]
               : "unknown";
}

/* Contract: refuses emitted-name readiness before the GGUF name-map row. */
int yvex_gguf_name_map_role_supported(const char *role, const char **reason)
{
    if (!role || !role[0]) {
        if (reason) *reason = "missing runtime role for GGUF emitted name";
        return 0;
    }
    if (reason) *reason = "string-only role admission is ambiguous; use the typed DeepSeek mapping plan";
    return 0;
}
