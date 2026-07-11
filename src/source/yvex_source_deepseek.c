/*
 * yvex_source_deepseek.c - DeepSeek source-sidecar fact extraction.
 *
 * Owner: src/source.
 * Owns: exact raw DeepSeek config, tokenizer, and generation metadata parsing.
 * Does not own: source IO, provenance, shard inventory, architecture IR, or rendering.
 * Invariants: required fields are explicit; absent and malformed facts are not defaulted.
 * Boundary: parsed sidecars do not make the target executable or supported.
 */
#include "yvex_source_deepseek.h"

#include "yvex_source_json.h"
#include "yvex_source_verify_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned long long seen;
    int architecture_matches;
} source_config_parse_state;

#define CONFIG_MODEL_TYPE (1ull << 0)
#define CONFIG_ARCHITECTURES (1ull << 1)
#define CONFIG_HIDDEN_SIZE (1ull << 2)
#define CONFIG_LAYER_COUNT (1ull << 3)
#define CONFIG_ATTENTION_HEADS (1ull << 4)
#define CONFIG_KV_HEADS (1ull << 5)
#define CONFIG_HEAD_DIM (1ull << 6)
#define CONFIG_QK_ROPE_DIM (1ull << 7)
#define CONFIG_MAX_POSITION (1ull << 8)
#define CONFIG_MOE_INTERMEDIATE (1ull << 9)
#define CONFIG_ROUTED_EXPERTS (1ull << 10)
#define CONFIG_SHARED_EXPERTS (1ull << 11)
#define CONFIG_EXPERTS_PER_TOKEN (1ull << 12)
#define CONFIG_HASH_LAYERS (1ull << 13)
#define CONFIG_Q_LORA (1ull << 14)
#define CONFIG_O_LORA (1ull << 15)
#define CONFIG_VOCAB (1ull << 16)
#define CONFIG_SLIDING_WINDOW (1ull << 17)
#define CONFIG_TIED (1ull << 18)
#define CONFIG_TORCH_DTYPE (1ull << 19)
#define CONFIG_EXPERT_DTYPE (1ull << 20)
#define CONFIG_HIDDEN_ACT (1ull << 21)
#define CONFIG_ROPE_SCALING (1ull << 22)
#define CONFIG_QUANTIZATION (1ull << 23)
#define CONFIG_ATTENTION_BIAS (1ull << 24)
#define CONFIG_ATTENTION_DROPOUT (1ull << 25)
#define CONFIG_BOS_TOKEN (1ull << 26)
#define CONFIG_EOS_TOKEN (1ull << 27)
#define CONFIG_COMPRESS_RATIOS (1ull << 28)
#define CONFIG_COMPRESS_ROPE_THETA (1ull << 29)
#define CONFIG_HC_EPS (1ull << 30)
#define CONFIG_HC_MULT (1ull << 31)
#define CONFIG_HC_SINKHORN_ITERS (1ull << 32)
#define CONFIG_INDEX_HEAD_DIM (1ull << 33)
#define CONFIG_INDEX_HEADS (1ull << 34)
#define CONFIG_INDEX_TOPK (1ull << 35)
#define CONFIG_NEXTN_LAYERS (1ull << 36)
#define CONFIG_O_GROUPS (1ull << 37)
#define CONFIG_RMS_NORM_EPS (1ull << 38)
#define CONFIG_ROPE_THETA (1ull << 39)
#define CONFIG_ROUTED_SCALING (1ull << 40)
#define CONFIG_SCORING_FUNC (1ull << 41)
#define CONFIG_TOPK_METHOD (1ull << 42)
#define CONFIG_NORM_TOPK (1ull << 43)
#define CONFIG_SWIGLU_LIMIT (1ull << 44)
#define CONFIG_USE_CACHE (1ull << 45)
#define CONFIG_REQUIRED_MASK ((1ull << 46) - 1ull)

static int source_config_mark(source_config_parse_state *state,
                              unsigned long long field)
{
    if (state->seen & field) return 0;
    state->seen |= field;
    return 1;
}

/* Parses the architecture list and records the canonical identity match. */
static int source_parse_architectures(
    yvex_source_json *json,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out,
    source_config_parse_state *state)
{
    char architecture[128];

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (!yvex_source_json_string(json, architecture,
                                     sizeof(architecture))) return 0;
        if (!out->architecture[0]) {
            snprintf(out->architecture, sizeof(out->architecture), "%s",
                     architecture);
        }
        if (strcmp(architecture, identity->config_architecture) == 0) {
            state->architecture_matches = 1;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses required raw RoPE scaling facts without runtime defaults. */
static int source_parse_rope_scaling(yvex_source_json *json,
                                     yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 7u;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "type") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->rope_scaling_type,
                    sizeof(out->rope_scaling_type))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "factor") == 0) {
            if ((seen & 2u) || !yvex_source_json_u64(
                    json, &out->rope_scaling_factor)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "original_max_position_embeddings") == 0) {
            if ((seen & 4u) || !yvex_source_json_u64(
                    json, &out->rope_original_context)) return 0;
            seen |= 4u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses source quantization metadata without choosing a release qtype. */
static int source_parse_quantization(yvex_source_json *json,
                                     yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 7u;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "quant_method") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->quant_method, sizeof(out->quant_method))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "fmt") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    json, out->quant_format, sizeof(out->quant_format))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "weight_block_size") == 0) {
            if (seen & 4u) return 0;
            yvex_source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != '[' ||
                !yvex_source_json_u64(json, &out->quant_block_rows)) return 0;
            yvex_source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != ',' ||
                !yvex_source_json_u64(json, &out->quant_block_columns)) return 0;
            yvex_source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != ']') return 0;
            seen |= 4u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses every required raw DeepSeek configuration field. */
static int source_parse_config_json(
    const char *data,
    size_t length,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out)
{
    yvex_source_json json;
    source_config_parse_state state;
    char key[YVEX_SOURCE_JSON_KEY_CAP];

    yvex_source_json_init(&json, data, length);
    memset(&state, 0, sizeof(state));
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        unsigned long long field = 0u;
        unsigned long long *number = NULL;
        char *text = NULL;
        size_t text_cap = 0u;

        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "model_type") == 0) {
            field = CONFIG_MODEL_TYPE; text = out->model_type; text_cap = sizeof(out->model_type);
        } else if (strcmp(key, "architectures") == 0) {
            field = CONFIG_ARCHITECTURES;
        } else if (strcmp(key, "hidden_size") == 0) {
            field = CONFIG_HIDDEN_SIZE; number = &out->hidden_size;
        } else if (strcmp(key, "num_hidden_layers") == 0) {
            field = CONFIG_LAYER_COUNT; number = &out->num_hidden_layers;
        } else if (strcmp(key, "num_attention_heads") == 0) {
            field = CONFIG_ATTENTION_HEADS; number = &out->num_attention_heads;
        } else if (strcmp(key, "num_key_value_heads") == 0) {
            field = CONFIG_KV_HEADS; number = &out->num_key_value_heads;
        } else if (strcmp(key, "head_dim") == 0) {
            field = CONFIG_HEAD_DIM; number = &out->head_dim;
        } else if (strcmp(key, "qk_rope_head_dim") == 0) {
            field = CONFIG_QK_ROPE_DIM; number = &out->qk_rope_head_dim;
        } else if (strcmp(key, "max_position_embeddings") == 0) {
            field = CONFIG_MAX_POSITION; number = &out->max_position_embeddings;
        } else if (strcmp(key, "moe_intermediate_size") == 0) {
            field = CONFIG_MOE_INTERMEDIATE; number = &out->moe_intermediate_size;
        } else if (strcmp(key, "n_routed_experts") == 0) {
            field = CONFIG_ROUTED_EXPERTS; number = &out->n_routed_experts;
        } else if (strcmp(key, "n_shared_experts") == 0) {
            field = CONFIG_SHARED_EXPERTS; number = &out->n_shared_experts;
        } else if (strcmp(key, "num_experts_per_tok") == 0) {
            field = CONFIG_EXPERTS_PER_TOKEN; number = &out->num_experts_per_tok;
        } else if (strcmp(key, "num_hash_layers") == 0) {
            field = CONFIG_HASH_LAYERS; number = &out->num_hash_layers;
        } else if (strcmp(key, "q_lora_rank") == 0) {
            field = CONFIG_Q_LORA; number = &out->q_lora_rank;
        } else if (strcmp(key, "o_lora_rank") == 0) {
            field = CONFIG_O_LORA; number = &out->o_lora_rank;
        } else if (strcmp(key, "vocab_size") == 0) {
            field = CONFIG_VOCAB; number = &out->vocab_size;
        } else if (strcmp(key, "sliding_window") == 0) {
            field = CONFIG_SLIDING_WINDOW; number = &out->sliding_window;
        } else if (strcmp(key, "tie_word_embeddings") == 0) {
            field = CONFIG_TIED;
        } else if (strcmp(key, "torch_dtype") == 0) {
            field = CONFIG_TORCH_DTYPE; text = out->torch_dtype; text_cap = sizeof(out->torch_dtype);
        } else if (strcmp(key, "expert_dtype") == 0) {
            field = CONFIG_EXPERT_DTYPE; text = out->expert_dtype; text_cap = sizeof(out->expert_dtype);
        } else if (strcmp(key, "hidden_act") == 0) {
            field = CONFIG_HIDDEN_ACT; text = out->hidden_act; text_cap = sizeof(out->hidden_act);
        } else if (strcmp(key, "rope_scaling") == 0) {
            field = CONFIG_ROPE_SCALING;
        } else if (strcmp(key, "quantization_config") == 0) {
            field = CONFIG_QUANTIZATION;
        } else if (strcmp(key, "attention_bias") == 0) {
            field = CONFIG_ATTENTION_BIAS;
        } else if (strcmp(key, "attention_dropout") == 0) {
            field = CONFIG_ATTENTION_DROPOUT;
        } else if (strcmp(key, "bos_token_id") == 0) {
            field = CONFIG_BOS_TOKEN; number = &out->bos_token_id;
        } else if (strcmp(key, "eos_token_id") == 0) {
            field = CONFIG_EOS_TOKEN; number = &out->eos_token_id;
        } else if (strcmp(key, "compress_ratios") == 0) {
            field = CONFIG_COMPRESS_RATIOS;
        } else if (strcmp(key, "compress_rope_theta") == 0) {
            field = CONFIG_COMPRESS_ROPE_THETA; number = &out->compress_rope_theta;
        } else if (strcmp(key, "hc_eps") == 0) {
            field = CONFIG_HC_EPS;
        } else if (strcmp(key, "hc_mult") == 0) {
            field = CONFIG_HC_MULT; number = &out->hc_mult;
        } else if (strcmp(key, "hc_sinkhorn_iters") == 0) {
            field = CONFIG_HC_SINKHORN_ITERS; number = &out->hc_sinkhorn_iters;
        } else if (strcmp(key, "index_head_dim") == 0) {
            field = CONFIG_INDEX_HEAD_DIM; number = &out->index_head_dim;
        } else if (strcmp(key, "index_n_heads") == 0) {
            field = CONFIG_INDEX_HEADS; number = &out->index_n_heads;
        } else if (strcmp(key, "index_topk") == 0) {
            field = CONFIG_INDEX_TOPK; number = &out->index_topk;
        } else if (strcmp(key, "num_nextn_predict_layers") == 0) {
            field = CONFIG_NEXTN_LAYERS; number = &out->num_nextn_predict_layers;
        } else if (strcmp(key, "o_groups") == 0) {
            field = CONFIG_O_GROUPS; number = &out->o_groups;
        } else if (strcmp(key, "rms_norm_eps") == 0) {
            field = CONFIG_RMS_NORM_EPS;
        } else if (strcmp(key, "rope_theta") == 0) {
            field = CONFIG_ROPE_THETA; number = &out->rope_theta;
        } else if (strcmp(key, "routed_scaling_factor") == 0) {
            field = CONFIG_ROUTED_SCALING;
        } else if (strcmp(key, "scoring_func") == 0) {
            field = CONFIG_SCORING_FUNC; text = out->scoring_func; text_cap = sizeof(out->scoring_func);
        } else if (strcmp(key, "topk_method") == 0) {
            field = CONFIG_TOPK_METHOD; text = out->topk_method; text_cap = sizeof(out->topk_method);
        } else if (strcmp(key, "norm_topk_prob") == 0) {
            field = CONFIG_NORM_TOPK;
        } else if (strcmp(key, "swiglu_limit") == 0) {
            field = CONFIG_SWIGLU_LIMIT;
        } else if (strcmp(key, "use_cache") == 0) {
            field = CONFIG_USE_CACHE;
        }
        if (!field) {
            if (!yvex_source_json_skip_value(&json)) return 0;
        } else if (!source_config_mark(&state, field)) {
            return 0;
        } else if (field == CONFIG_ARCHITECTURES) {
            if (!source_parse_architectures(&json, identity, out, &state)) return 0;
        } else if (field == CONFIG_TIED) {
            if (!yvex_source_json_bool(&json, &out->tie_word_embeddings)) return 0;
        } else if (field == CONFIG_ATTENTION_BIAS) {
            if (!yvex_source_json_bool(&json, &out->attention_bias)) return 0;
        } else if (field == CONFIG_NORM_TOPK) {
            if (!yvex_source_json_bool(&json, &out->norm_topk_prob)) return 0;
        } else if (field == CONFIG_USE_CACHE) {
            if (!yvex_source_json_bool(&json, &out->use_cache)) return 0;
        } else if (field == CONFIG_ATTENTION_DROPOUT) {
            if (!yvex_source_json_number_text(&json, out->attention_dropout,
                                               sizeof(out->attention_dropout))) return 0;
        } else if (field == CONFIG_HC_EPS) {
            if (!yvex_source_json_number_text(&json, out->hc_eps,
                                               sizeof(out->hc_eps))) return 0;
        } else if (field == CONFIG_RMS_NORM_EPS) {
            if (!yvex_source_json_number_text(&json, out->rms_norm_eps,
                                               sizeof(out->rms_norm_eps))) return 0;
        } else if (field == CONFIG_ROUTED_SCALING) {
            if (!yvex_source_json_number_text(&json, out->routed_scaling_factor,
                                               sizeof(out->routed_scaling_factor))) return 0;
        } else if (field == CONFIG_SWIGLU_LIMIT) {
            if (!yvex_source_json_number_text(&json, out->swiglu_limit,
                                               sizeof(out->swiglu_limit))) return 0;
        } else if (field == CONFIG_COMPRESS_RATIOS) {
            if (!yvex_source_json_u64_array(
                    &json, out->compress_ratios,
                    YVEX_SOURCE_VERIFY_COMPRESS_RATIO_CAP,
                    &out->compress_ratio_count)) return 0;
        } else if (field == CONFIG_ROPE_SCALING) {
            if (!source_parse_rope_scaling(&json, out)) return 0;
        } else if (field == CONFIG_QUANTIZATION) {
            if (!source_parse_quantization(&json, out)) return 0;
        } else if (number) {
            if (!yvex_source_json_u64(&json, number)) return 0;
        } else if (!yvex_source_json_string(&json, text, text_cap)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    if (!yvex_source_json_complete(&json)) return 0;
    if (state.seen != CONFIG_REQUIRED_MASK) {
        yvex_source_verification_add_blocker(out, "missing-source-config-fact");
    }
    if ((state.seen & CONFIG_MODEL_TYPE) &&
        strcmp(out->model_type, identity->config_model_type) != 0) {
        yvex_source_verification_add_blocker(out, "wrong-source-model-type");
    }
    if ((state.seen & CONFIG_ARCHITECTURES) && !state.architecture_matches) {
        yvex_source_verification_add_blocker(out, "wrong-source-architecture");
    }
    out->config_valid = state.seen == CONFIG_REQUIRED_MASK &&
                        strcmp(out->model_type, identity->config_model_type) == 0 &&
                        state.architecture_matches &&
                        out->compress_ratio_count > 0u;
    return 1;
}

/* Parses required tokenizer configuration facts without loading a tokenizer. */
static int source_parse_tokenizer_config(const char *data,
                                         size_t length,
                                         yvex_source_verification *out)
{
    yvex_source_json json;
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    yvex_source_json_init(&json, data, length);
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "tokenizer_class") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    &json, out->tokenizer_class,
                    sizeof(out->tokenizer_class))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "model_max_length") == 0) {
            if ((seen & 2u) || !yvex_source_json_u64(
                    &json, &out->tokenizer_model_max_length)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "bos_token") == 0) {
            if ((seen & 4u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "eos_token") == 0) {
            if ((seen & 8u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 8u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return yvex_source_json_complete(&json) && seen == 15u;
}

/* Parses raw generation-sidecar policy without implementing generation. */
static int source_parse_generation_config(const char *data,
                                          size_t length,
                                          yvex_source_verification *out)
{
    yvex_source_json json;
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    yvex_source_json_init(&json, data, length);
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "_from_model_config") == 0) {
            if ((seen & 1u) || !yvex_source_json_bool(
                    &json, &out->generation_from_model_config)) return 0;
            seen |= 1u;
        } else if (strcmp(key, "bos_token_id") == 0) {
            if ((seen & 2u) || !yvex_source_json_u64(
                    &json, &out->generation_bos_token_id)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "eos_token_id") == 0) {
            if ((seen & 4u) || !yvex_source_json_u64(
                    &json, &out->generation_eos_token_id)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "do_sample") == 0) {
            if ((seen & 8u) || !yvex_source_json_bool(
                    &json, &out->generation_do_sample)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "temperature") == 0) {
            if ((seen & 16u) || !yvex_source_json_number_text(
                    &json, out->generation_temperature,
                    sizeof(out->generation_temperature))) return 0;
            seen |= 16u;
        } else if (strcmp(key, "top_p") == 0) {
            if ((seen & 32u) || !yvex_source_json_number_text(
                    &json, out->generation_top_p,
                    sizeof(out->generation_top_p))) return 0;
            seen |= 32u;
        } else if (strcmp(key, "transformers_version") == 0) {
            if ((seen & 64u) || !yvex_source_json_string(
                    &json, out->generation_transformers_version,
                    sizeof(out->generation_transformers_version))) return 0;
            seen |= 64u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return yvex_source_json_complete(&json) && seen == 127u;
}

/* Parses tokenizer model type and requires a structured vocabulary object. */
static int source_parse_tokenizer_model(yvex_source_json *json,
                                        yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 3u;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "type") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->tokenizer_model_type,
                    sizeof(out->tokenizer_model_type))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "vocab") == 0) {
            if ((seen & 2u) || !yvex_source_json_skip_object(json)) return 0;
            seen |= 2u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Validates required tokenizer JSON structure and preserves its model type. */
static int source_parse_tokenizer_json(const char *data,
                                       size_t length,
                                       yvex_source_verification *out)
{
    yvex_source_json json;
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    char version[16];
    unsigned int seen = 0u;

    yvex_source_json_init(&json, data, length);
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "version") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    &json, version, sizeof(version)) ||
                strcmp(version, "1.0") != 0) return 0;
            seen |= 1u;
        } else if (strcmp(key, "added_tokens") == 0) {
            if ((seen & 2u) || !yvex_source_json_skip_array(&json)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "normalizer") == 0) {
            if ((seen & 4u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "pre_tokenizer") == 0) {
            if ((seen & 8u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "post_processor") == 0) {
            if ((seen & 16u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "decoder") == 0) {
            if ((seen & 32u) || !yvex_source_json_skip_value(&json)) return 0;
            seen |= 32u;
        } else if (strcmp(key, "model") == 0) {
            if ((seen & 64u) || !source_parse_tokenizer_model(&json, out)) return 0;
            seen |= 64u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return yvex_source_json_complete(&json) && seen == 127u &&
           out->tokenizer_model_type[0] != '\0';
}

int yvex_source_deepseek_parse_sidecar(
    yvex_source_deepseek_sidecar_kind kind,
    const char *data,
    size_t length,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out)
{
    int valid;

    if (!data || !identity || !out) return 0;
    switch (kind) {
    case YVEX_SOURCE_DEEPSEEK_CONFIG:
        return source_parse_config_json(data, length, identity, out);
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER:
        valid = source_parse_tokenizer_json(data, length, out);
        out->tokenizer_json_valid = valid;
        return valid;
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER_CONFIG:
        valid = source_parse_tokenizer_config(data, length, out);
        out->tokenizer_config_valid = valid;
        return valid;
    case YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG:
        valid = source_parse_generation_config(data, length, out);
        out->generation_config_valid = valid;
        return valid;
    default:
        return 0;
    }
}
