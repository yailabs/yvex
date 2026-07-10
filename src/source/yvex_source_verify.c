/*
 * yvex_source_verify.c - exact local source verifier.
 *
 * Owner:
 *   src/source
 *
 * Owns:
 *   structured manifest, config, tokenizer, shard-index, Hugging Face revision,
 *   root-shard, safetensors-header, and footprint verification coordination.
 *
 * Does not own:
 *   target selection, tensor payload reads, architecture IR, tensor roles,
 *   quantization, GGUF emission, materialization, runtime, or generation.
 *
 * Invariants:
 *   JSON is parsed structurally; every byte read is metadata or a safetensors
 *   header; ordinary source defects become typed blockers rather than inferred
 *   defaults.
 *
 * Boundary:
 *   a verified source remains an unsupported model target.
 */
#define _XOPEN_SOURCE 700
#include "yvex_source_verify.h"

#include "yvex_source_private.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SOURCE_JSON_DEPTH_CAP 64u
#define SOURCE_JSON_KEY_CAP 1024u
#define SOURCE_CONFIG_CAP (1024u * 1024u)
#define SOURCE_TOKENIZER_CAP (32u * 1024u * 1024u)
#define SOURCE_INDEX_CAP (128u * 1024u * 1024u)
#define SOURCE_MANIFEST_CAP (32u * 1024u * 1024u)

typedef struct {
    const char *cursor;
    const char *end;
    unsigned int depth;
} source_json;

typedef struct {
    char *tensor;
    char *shard;
    int seen_in_header;
} source_index_entry;

typedef struct {
    source_index_entry *items;
    size_t count;
    size_t cap;
    unsigned long long declared_total_size;
    int has_declared_total_size;
    int duplicate_tensor;
} source_index;

typedef struct {
    char **names;
    unsigned long long *sizes;
    size_t count;
    size_t cap;
    unsigned int declared_total;
} source_shards;

typedef struct {
    unsigned long long seen;
    int architecture_matches;
} source_config_parse_state;

enum {
    SOURCE_JSON_CONFIG = 0,
    SOURCE_JSON_TOKENIZER,
    SOURCE_JSON_TOKENIZER_CONFIG,
    SOURCE_JSON_GENERATION_CONFIG
};

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

static char *source_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) return NULL;
    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1u);
    return copy;
}

static int source_path_join(char *out, size_t cap,
                            const char *left, const char *right)
{
    int n;

    if (!out || cap == 0u || !left || !right) return 0;
    n = snprintf(out, cap, "%s%s%s", left,
                 left[0] && left[strlen(left) - 1u] == '/' ? "" : "/",
                 right);
    return n >= 0 && (size_t)n < cap;
}

static int source_regular_file(const char *path, unsigned long long *size)
{
    struct stat st;

    if (!path || lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    if (st.st_size < 0) return 0;
    if (size) *size = (unsigned long long)st.st_size;
    return 1;
}

int yvex_source_checked_add_u64(unsigned long long *total,
                                unsigned long long value)
{
    if (!total || ULLONG_MAX - *total < value) return 0;
    *total += value;
    return 1;
}

static void source_add_blocker(yvex_source_verification *out,
                               const char *reason)
{
    unsigned int i;

    if (!out || !reason) return;
    for (i = 0; i < out->blocker_count; ++i) {
        if (strcmp(out->blockers[i], reason) == 0) return;
    }
    if (out->blocker_count < YVEX_SOURCE_VERIFY_BLOCKER_CAP) {
        out->blockers[out->blocker_count++] = reason;
    }
}

static int source_has_blocker(const yvex_source_verification *out,
                              const char *reason)
{
    unsigned int i;

    if (!out || !reason) return 0;
    for (i = 0; i < out->blocker_count; ++i) {
        if (strcmp(out->blockers[i], reason) == 0) return 1;
    }
    return 0;
}

/*
 * source_read_bounded_file()
 *
 * Allocates and reads one metadata file up to the caller's hard cap. The
 * caller owns the returned buffer. Missing files return NULL without an error;
 * bounds, allocation, read, and close failures populate err.
 */
static char *source_read_bounded_file(const char *path, size_t cap,
                                      size_t *length, yvex_error *err)
{
    struct stat st;
    FILE *fp;
    char *data;
    size_t wanted;
    int read_failed;

    if (length) *length = 0u;
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        return NULL;
    }
    if ((unsigned long long)st.st_size > (unsigned long long)cap) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "source_verify_read",
                        "metadata file exceeds bounded read limit: %s", path);
        return NULL;
    }
    wanted = (size_t)st.st_size;
    data = (char *)malloc(wanted + 1u);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_read",
                       "metadata buffer allocation failed");
        return NULL;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        free(data);
        yvex_error_setf(err, YVEX_ERR_IO, "source_verify_read",
                        "cannot read metadata file: %s", path);
        return NULL;
    }
    read_failed = (wanted && fread(data, 1u, wanted, fp) != wanted) ||
                  ferror(fp);
    if (fclose(fp) != 0) read_failed = 1;
    if (read_failed) {
        free(data);
        yvex_error_setf(err, YVEX_ERR_IO, "source_verify_read",
                        "cannot read metadata file: %s", path);
        return NULL;
    }
    data[wanted] = '\0';
    if (length) *length = wanted;
    return data;
}

static void source_json_space(source_json *json)
{
    while (json->cursor < json->end &&
           isspace((unsigned char)*json->cursor)) {
        json->cursor++;
    }
}

static int source_json_hex(char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

/*
 * source_json_string()
 *
 * Consumes one JSON string from caller-owned memory and optionally copies its
 * decoded bounded value. It allocates nothing and rejects malformed escapes,
 * control bytes, and truncation.
 */
static int source_json_string(source_json *json, char *out, size_t cap)
{
    size_t length = 0u;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '"') return 0;
    while (json->cursor < json->end) {
        unsigned char ch = (unsigned char)*json->cursor++;
        if (ch == '"') {
            if (out && cap) out[length < cap ? length : cap - 1u] = '\0';
            return !out || length < cap;
        }
        if (ch < 0x20u) return 0;
        if (ch == '\\') {
            unsigned char escaped;
            if (json->cursor >= json->end) return 0;
            escaped = (unsigned char)*json->cursor++;
            if (escaped == 'u') {
                unsigned int i;
                for (i = 0; i < 4u; ++i) {
                    if (json->cursor >= json->end ||
                        !source_json_hex(*json->cursor++)) return 0;
                }
                ch = '?';
            } else if (!strchr("\"\\/bfnrt", escaped)) {
                return 0;
            } else {
                ch = escaped;
            }
        }
        if (out && length + 1u < cap) out[length] = (char)ch;
        length++;
    }
    return 0;
}

static int source_json_skip_value(source_json *json);

/* Consumes one bounded-depth JSON array without retaining its values. */
static int source_json_skip_array(source_json *json)
{
    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[' ||
        json->depth >= SOURCE_JSON_DEPTH_CAP) return 0;
    json->depth++;
    source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!source_json_skip_value(json)) return 0;
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            json->depth--;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Consumes one bounded-depth JSON object without retaining its fields. */
static int source_json_skip_object(source_json *json)
{
    char key[SOURCE_JSON_KEY_CAP];

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{' ||
        json->depth >= SOURCE_JSON_DEPTH_CAP) return 0;
    json->depth++;
    source_json_space(json);
    if (json->cursor < json->end && *json->cursor == '}') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (!source_json_skip_value(json)) return 0;
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') {
            json->cursor++;
            json->depth--;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Consumes one syntactically valid JSON number without conversion. */
static int source_json_skip_number(source_json *json)
{
    const char *start;

    source_json_space(json);
    start = json->cursor;
    if (json->cursor < json->end && *json->cursor == '-') json->cursor++;
    if (json->cursor >= json->end) return 0;
    if (*json->cursor == '0') {
        json->cursor++;
    } else {
        if (!isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    if (json->cursor < json->end && *json->cursor == '.') {
        json->cursor++;
        if (json->cursor >= json->end ||
            !isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    if (json->cursor < json->end &&
        (*json->cursor == 'e' || *json->cursor == 'E')) {
        json->cursor++;
        if (json->cursor < json->end &&
            (*json->cursor == '+' || *json->cursor == '-')) json->cursor++;
        if (json->cursor >= json->end ||
            !isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    return json->cursor > start;
}

static int source_json_literal(source_json *json, const char *literal)
{
    size_t length = strlen(literal);

    source_json_space(json);
    if ((size_t)(json->end - json->cursor) < length ||
        memcmp(json->cursor, literal, length) != 0) return 0;
    json->cursor += length;
    return 1;
}

/* Dispatches one JSON value while enforcing the shared recursion cap. */
static int source_json_skip_value(source_json *json)
{
    source_json_space(json);
    if (json->cursor >= json->end) return 0;
    if (*json->cursor == '{') return source_json_skip_object(json);
    if (*json->cursor == '[') return source_json_skip_array(json);
    if (*json->cursor == '"') return source_json_string(json, NULL, 0u);
    if (*json->cursor == 't') return source_json_literal(json, "true");
    if (*json->cursor == 'f') return source_json_literal(json, "false");
    if (*json->cursor == 'n') return source_json_literal(json, "null");
    return source_json_skip_number(json);
}

static int source_json_complete(source_json *json)
{
    source_json_space(json);
    return json->cursor == json->end && json->depth == 0u;
}

static int source_json_u64(source_json *json, unsigned long long *out)
{
    unsigned long long value = 0u;

    source_json_space(json);
    if (json->cursor >= json->end ||
        !isdigit((unsigned char)*json->cursor)) return 0;
    while (json->cursor < json->end &&
           isdigit((unsigned char)*json->cursor)) {
        unsigned int digit = (unsigned int)(*json->cursor++ - '0');
        if (value > (ULLONG_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    *out = value;
    return 1;
}

static int source_json_bool(source_json *json, int *out)
{
    if (source_json_literal(json, "true")) {
        *out = 1;
        return 1;
    }
    if (source_json_literal(json, "false")) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int source_json_number_text(source_json *json, char *out, size_t cap)
{
    const char *start;
    size_t length;

    if (!json || !out || cap == 0u) return 0;
    source_json_space(json);
    start = json->cursor;
    if (!source_json_skip_number(json)) return 0;
    length = (size_t)(json->cursor - start);
    if (length == 0u || length >= cap) return 0;
    memcpy(out, start, length);
    out[length] = '\0';
    return 1;
}

/*
 * source_json_u64_array()
 *
 * Parses a non-empty bounded array into caller-owned storage. It allocates
 * nothing and rejects negative, overflowing, malformed, or oversized values.
 */
static int source_json_u64_array(source_json *json,
                                 unsigned long long *values,
                                 size_t cap,
                                 unsigned long long *count)
{
    unsigned long long used = 0u;

    if (!json || !values || !count || cap == 0u) return 0;
    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (used >= (unsigned long long)cap ||
            !source_json_u64(json, &values[used])) return 0;
        used++;
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            *count = used;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

static int source_config_mark(source_config_parse_state *state,
                              unsigned long long field)
{
    if (state->seen & field) return 0;
    state->seen |= field;
    return 1;
}

/* Parses the architecture list and records exact canonical identity matches. */
static int source_parse_architectures(source_json *json,
                                      const yvex_model_target_identity *identity,
                                      yvex_source_verification *out,
                                      source_config_parse_state *state)
{
    char architecture[128];

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (!source_json_string(json, architecture, sizeof(architecture))) return 0;
        if (!out->architecture[0]) {
            snprintf(out->architecture, sizeof(out->architecture), "%s",
                     architecture);
        }
        if (strcmp(architecture, identity->config_architecture) == 0) {
            state->architecture_matches = 1;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses required raw RoPE scaling facts without applying runtime defaults. */
static int source_parse_rope_scaling(source_json *json,
                                     yvex_source_verification *out)
{
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 7u;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "type") == 0) {
            if ((seen & 1u) || !source_json_string(json, out->rope_scaling_type,
                                                  sizeof(out->rope_scaling_type))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "factor") == 0) {
            if ((seen & 2u) || !source_json_u64(json, &out->rope_scaling_factor)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "original_max_position_embeddings") == 0) {
            if ((seen & 4u) || !source_json_u64(json, &out->rope_original_context)) return 0;
            seen |= 4u;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses source quantization metadata without selecting a release qtype. */
static int source_parse_quantization(source_json *json,
                                     yvex_source_verification *out)
{
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 7u;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "quant_method") == 0) {
            if ((seen & 1u) || !source_json_string(json, out->quant_method,
                                                  sizeof(out->quant_method))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "fmt") == 0) {
            if ((seen & 2u) || !source_json_string(json, out->quant_format,
                                                  sizeof(out->quant_format))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "weight_block_size") == 0) {
            if (seen & 4u) return 0;
            source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != '[' ||
                !source_json_u64(json, &out->quant_block_rows)) return 0;
            source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != ',' ||
                !source_json_u64(json, &out->quant_block_columns)) return 0;
            source_json_space(json);
            if (json->cursor >= json->end || *json->cursor++ != ']') return 0;
            seen |= 4u;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/*
 * source_parse_config_json()
 *
 * Parses every required raw DeepSeek configuration fact into caller-owned
 * output. It allocates nothing, rejects duplicate or malformed known fields,
 * and records missing or wrong identity facts without creating defaults.
 */
static int source_parse_config_json(const char *data, size_t length,
                                    const yvex_model_target_identity *identity,
                                    yvex_source_verification *out)
{
    source_json json = {data, data + length, 0u};
    source_config_parse_state state;
    char key[SOURCE_JSON_KEY_CAP];

    memset(&state, 0, sizeof(state));
    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        unsigned long long field = 0u;
        unsigned long long *number = NULL;
        char *text = NULL;
        size_t text_cap = 0u;

        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
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
            field = CONFIG_COMPRESS_ROPE_THETA;
            number = &out->compress_rope_theta;
        } else if (strcmp(key, "hc_eps") == 0) {
            field = CONFIG_HC_EPS;
        } else if (strcmp(key, "hc_mult") == 0) {
            field = CONFIG_HC_MULT; number = &out->hc_mult;
        } else if (strcmp(key, "hc_sinkhorn_iters") == 0) {
            field = CONFIG_HC_SINKHORN_ITERS;
            number = &out->hc_sinkhorn_iters;
        } else if (strcmp(key, "index_head_dim") == 0) {
            field = CONFIG_INDEX_HEAD_DIM; number = &out->index_head_dim;
        } else if (strcmp(key, "index_n_heads") == 0) {
            field = CONFIG_INDEX_HEADS; number = &out->index_n_heads;
        } else if (strcmp(key, "index_topk") == 0) {
            field = CONFIG_INDEX_TOPK; number = &out->index_topk;
        } else if (strcmp(key, "num_nextn_predict_layers") == 0) {
            field = CONFIG_NEXTN_LAYERS;
            number = &out->num_nextn_predict_layers;
        } else if (strcmp(key, "o_groups") == 0) {
            field = CONFIG_O_GROUPS; number = &out->o_groups;
        } else if (strcmp(key, "rms_norm_eps") == 0) {
            field = CONFIG_RMS_NORM_EPS;
        } else if (strcmp(key, "rope_theta") == 0) {
            field = CONFIG_ROPE_THETA; number = &out->rope_theta;
        } else if (strcmp(key, "routed_scaling_factor") == 0) {
            field = CONFIG_ROUTED_SCALING;
        } else if (strcmp(key, "scoring_func") == 0) {
            field = CONFIG_SCORING_FUNC;
            text = out->scoring_func; text_cap = sizeof(out->scoring_func);
        } else if (strcmp(key, "topk_method") == 0) {
            field = CONFIG_TOPK_METHOD;
            text = out->topk_method; text_cap = sizeof(out->topk_method);
        } else if (strcmp(key, "norm_topk_prob") == 0) {
            field = CONFIG_NORM_TOPK;
        } else if (strcmp(key, "swiglu_limit") == 0) {
            field = CONFIG_SWIGLU_LIMIT;
        } else if (strcmp(key, "use_cache") == 0) {
            field = CONFIG_USE_CACHE;
        }
        if (!field) {
            if (!source_json_skip_value(&json)) return 0;
        } else if (!source_config_mark(&state, field)) {
            return 0;
        } else if (field == CONFIG_ARCHITECTURES) {
            if (!source_parse_architectures(&json, identity, out, &state)) return 0;
        } else if (field == CONFIG_TIED) {
            if (!source_json_bool(&json, &out->tie_word_embeddings)) return 0;
        } else if (field == CONFIG_ATTENTION_BIAS) {
            if (!source_json_bool(&json, &out->attention_bias)) return 0;
        } else if (field == CONFIG_NORM_TOPK) {
            if (!source_json_bool(&json, &out->norm_topk_prob)) return 0;
        } else if (field == CONFIG_USE_CACHE) {
            if (!source_json_bool(&json, &out->use_cache)) return 0;
        } else if (field == CONFIG_ATTENTION_DROPOUT) {
            if (!source_json_number_text(&json, out->attention_dropout,
                                         sizeof(out->attention_dropout))) return 0;
        } else if (field == CONFIG_HC_EPS) {
            if (!source_json_number_text(&json, out->hc_eps,
                                         sizeof(out->hc_eps))) return 0;
        } else if (field == CONFIG_RMS_NORM_EPS) {
            if (!source_json_number_text(&json, out->rms_norm_eps,
                                         sizeof(out->rms_norm_eps))) return 0;
        } else if (field == CONFIG_ROUTED_SCALING) {
            if (!source_json_number_text(&json, out->routed_scaling_factor,
                                         sizeof(out->routed_scaling_factor))) return 0;
        } else if (field == CONFIG_SWIGLU_LIMIT) {
            if (!source_json_number_text(&json, out->swiglu_limit,
                                         sizeof(out->swiglu_limit))) return 0;
        } else if (field == CONFIG_COMPRESS_RATIOS) {
            if (!source_json_u64_array(
                    &json, out->compress_ratios,
                    YVEX_SOURCE_VERIFY_COMPRESS_RATIO_CAP,
                    &out->compress_ratio_count)) return 0;
        } else if (field == CONFIG_ROPE_SCALING) {
            if (!source_parse_rope_scaling(&json, out)) return 0;
        } else if (field == CONFIG_QUANTIZATION) {
            if (!source_parse_quantization(&json, out)) return 0;
        } else if (number) {
            if (!source_json_u64(&json, number)) return 0;
        } else if (!source_json_string(&json, text, text_cap)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    if (!source_json_complete(&json)) return 0;
    if (state.seen != CONFIG_REQUIRED_MASK) {
        source_add_blocker(out, "missing-source-config-fact");
    }
    if ((state.seen & CONFIG_MODEL_TYPE) &&
        strcmp(out->model_type, identity->config_model_type) != 0) {
        source_add_blocker(out, "wrong-source-model-type");
    }
    if ((state.seen & CONFIG_ARCHITECTURES) && !state.architecture_matches) {
        source_add_blocker(out, "wrong-source-architecture");
    }
    out->config_valid = state.seen == CONFIG_REQUIRED_MASK &&
                        strcmp(out->model_type,
                               identity->config_model_type) == 0 &&
                        state.architecture_matches &&
                        out->compress_ratio_count > 0u;
    return 1;
}

/* Parses required tokenizer sidecar facts without loading tokenizer runtime. */
static int source_parse_tokenizer_config(const char *data, size_t length,
                                         yvex_source_verification *out)
{
    source_json json = {data, data + length, 0u};
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "tokenizer_class") == 0) {
            if ((seen & 1u) || !source_json_string(&json, out->tokenizer_class,
                                                  sizeof(out->tokenizer_class))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "model_max_length") == 0) {
            if ((seen & 2u) || !source_json_u64(&json,
                                               &out->tokenizer_model_max_length)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "bos_token") == 0) {
            if ((seen & 4u) || !source_json_skip_value(&json)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "eos_token") == 0) {
            if ((seen & 8u) || !source_json_skip_value(&json)) return 0;
            seen |= 8u;
        } else if (!source_json_skip_value(&json)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return source_json_complete(&json) && seen == 15u;
}

/*
 * source_parse_generation_config()
 *
 * Parses raw generation-sidecar identity and policy facts. It allocates
 * nothing and does not implement sampling, tokenization, or generation.
 */
static int source_parse_generation_config(const char *data, size_t length,
                                          yvex_source_verification *out)
{
    source_json json = {data, data + length, 0u};
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "_from_model_config") == 0) {
            if ((seen & 1u) || !source_json_bool(
                    &json, &out->generation_from_model_config)) return 0;
            seen |= 1u;
        } else if (strcmp(key, "bos_token_id") == 0) {
            if ((seen & 2u) || !source_json_u64(
                    &json, &out->generation_bos_token_id)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "eos_token_id") == 0) {
            if ((seen & 4u) || !source_json_u64(
                    &json, &out->generation_eos_token_id)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "do_sample") == 0) {
            if ((seen & 8u) || !source_json_bool(
                    &json, &out->generation_do_sample)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "temperature") == 0) {
            if ((seen & 16u) || !source_json_number_text(
                    &json, out->generation_temperature,
                    sizeof(out->generation_temperature))) return 0;
            seen |= 16u;
        } else if (strcmp(key, "top_p") == 0) {
            if ((seen & 32u) || !source_json_number_text(
                    &json, out->generation_top_p,
                    sizeof(out->generation_top_p))) return 0;
            seen |= 32u;
        } else if (strcmp(key, "transformers_version") == 0) {
            if ((seen & 64u) || !source_json_string(
                    &json, out->generation_transformers_version,
                    sizeof(out->generation_transformers_version))) return 0;
            seen |= 64u;
        } else if (!source_json_skip_value(&json)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return source_json_complete(&json) && seen == 127u;
}

/* Parses the tokenizer model type and requires a structured vocabulary field. */
static int source_parse_tokenizer_model(source_json *json,
                                        yvex_source_verification *out)
{
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 3u;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "type") == 0) {
            if ((seen & 1u) ||
                !source_json_string(json, out->tokenizer_model_type,
                                    sizeof(out->tokenizer_model_type))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "vocab") == 0) {
            if ((seen & 2u) || !source_json_skip_object(json)) return 0;
            seen |= 2u;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/*
 * source_parse_tokenizer_json()
 *
 * Validates the required tokenizer JSON structure and preserves its model
 * type. It allocates nothing and does not execute tokenization.
 */
static int source_parse_tokenizer_json(const char *data, size_t length,
                                       yvex_source_verification *out)
{
    source_json json = {data, data + length, 0u};
    char key[SOURCE_JSON_KEY_CAP];
    char version[16];
    unsigned int seen = 0u;

    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "version") == 0) {
            if ((seen & 1u) ||
                !source_json_string(&json, version, sizeof(version)) ||
                strcmp(version, "1.0") != 0) return 0;
            seen |= 1u;
        } else if (strcmp(key, "added_tokens") == 0) {
            if ((seen & 2u) || !source_json_skip_array(&json)) return 0;
            seen |= 2u;
        } else if (strcmp(key, "normalizer") == 0) {
            if ((seen & 4u) || !source_json_skip_value(&json)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "pre_tokenizer") == 0) {
            if ((seen & 8u) || !source_json_skip_value(&json)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "post_processor") == 0) {
            if ((seen & 16u) || !source_json_skip_value(&json)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "decoder") == 0) {
            if ((seen & 32u) || !source_json_skip_value(&json)) return 0;
            seen |= 32u;
        } else if (strcmp(key, "model") == 0) {
            if ((seen & 64u) || !source_parse_tokenizer_model(&json, out)) return 0;
            seen |= 64u;
        } else if (!source_json_skip_value(&json)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return source_json_complete(&json) && seen == 127u &&
           out->tokenizer_model_type[0] != '\0';
}

/* Parses source kind, repository, and optional revision from one manifest. */
static int source_parse_manifest_source(source_json *json,
                                        yvex_source_verification *out)
{
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return (seen & 2u) == 2u;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "kind") == 0) {
            if ((seen & 1u) || !source_json_string(json, out->source_kind,
                                                  sizeof(out->source_kind))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "repo") == 0) {
            if ((seen & 2u) || !source_json_string(json, out->repository_id,
                                                  sizeof(out->repository_id))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "revision") == 0) {
            if ((seen & 4u) || !source_json_string(json, out->manifest_revision,
                                                  sizeof(out->manifest_revision))) return 0;
            seen |= 4u;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses the manifest local path into caller-owned bounded storage. */
static int source_parse_manifest_local(source_json *json, char *path, size_t cap)
{
    char key[SOURCE_JSON_KEY_CAP];
    int seen = 0;

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "path") == 0) {
            if (seen || !source_json_string(json, path, cap)) return 0;
            seen = 1;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/*
 * source_parse_manifest()
 *
 * Validates the complete source-manifest schema and extracts structured
 * provenance into caller-owned output. It performs no filesystem access.
 */
static int source_parse_manifest(const char *data, size_t length,
                                 yvex_source_verification *out,
                                 char *local_path, size_t local_path_cap)
{
    source_json json = {data, data + length, 0u};
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;

    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "schema") == 0) {
            char schema[64];
            if ((seen & 1u) || !source_json_string(&json, schema, sizeof(schema)) ||
                strcmp(schema, "yvex.source_manifest.v1") != 0) return 0;
            seen |= 1u;
        } else if (strcmp(key, "status") == 0) {
            if ((seen & 2u) || !source_json_string(&json, out->manifest_status,
                                                  sizeof(out->manifest_status))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "source") == 0) {
            if ((seen & 4u) || !source_parse_manifest_source(&json, out)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "local") == 0) {
            if ((seen & 8u) || !source_parse_manifest_local(&json, local_path,
                                                           local_path_cap)) return 0;
            seen |= 8u;
        } else if (!source_json_skip_value(&json)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return source_json_complete(&json) && seen == 15u;
}

/*
 * source_index_append()
 *
 * Grows the caller-owned index and copies one tensor-to-shard assignment.
 * Allocation failure leaves the existing rows owned by the index cleanup path.
 */
static int source_index_append(source_index *index,
                               const char *tensor, const char *shard,
                               yvex_error *err)
{
    source_index_entry *next;
    size_t cap;

    if (index->count == index->cap) {
        cap = index->cap ? index->cap * 2u : 256u;
        if (cap < index->cap || cap > SIZE_MAX / sizeof(index->items[0])) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "source_index",
                           "shard index allocation overflow");
            return YVEX_ERR_BOUNDS;
        }
        next = (source_index_entry *)realloc(index->items,
                                             cap * sizeof(index->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_index",
                           "shard index allocation failed");
            return YVEX_ERR_NOMEM;
        }
        index->items = next;
        index->cap = cap;
    }
    memset(&index->items[index->count], 0, sizeof(index->items[0]));
    index->items[index->count].tensor = source_strdup(tensor);
    index->items[index->count].shard = source_strdup(shard);
    if (!index->items[index->count].tensor || !index->items[index->count].shard) {
        free(index->items[index->count].tensor);
        free(index->items[index->count].shard);
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_index",
                       "shard index row allocation failed");
        return YVEX_ERR_NOMEM;
    }
    index->count++;
    return YVEX_OK;
}

static void source_index_free(source_index *index)
{
    size_t i;

    if (!index) return;
    for (i = 0; i < index->count; ++i) {
        free(index->items[i].tensor);
        free(index->items[i].shard);
    }
    free(index->items);
    memset(index, 0, sizeof(*index));
}

static int source_index_compare(const void *left, const void *right)
{
    const source_index_entry *a = (const source_index_entry *)left;
    const source_index_entry *b = (const source_index_entry *)right;
    return strcmp(a->tensor, b->tensor);
}

/* Parses optional index metadata and checked total-size facts. */
static int source_parse_index_metadata(source_json *json, source_index *index)
{
    char key[SOURCE_JSON_KEY_CAP];

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return 1;
        }
        if (!source_json_string(json, key, sizeof(key))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "total_size") == 0) {
            if (index->has_declared_total_size ||
                !source_json_u64(json, &index->declared_total_size)) return 0;
            index->has_declared_total_size = 1;
        } else if (!source_json_skip_value(json)) {
            return 0;
        }
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses and owns every tensor-to-shard index assignment. */
static int source_parse_weight_map(source_json *json, source_index *index,
                                   yvex_error *err)
{
    char tensor[SOURCE_JSON_KEY_CAP];
    char shard[YVEX_PATH_CAP];

    source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        int rc;
        source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return index->count > 0u;
        }
        if (!source_json_string(json, tensor, sizeof(tensor))) return 0;
        source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':' ||
            !source_json_string(json, shard, sizeof(shard))) return 0;
        rc = source_index_append(index, tensor, shard, err);
        if (rc != YVEX_OK) return -1;
        source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/*
 * source_parse_index_json()
 *
 * Parses the complete shard index, sorts copied rows for exact lookup, and
 * rejects duplicate tensor names. The caller owns all accumulated rows.
 */
static int source_parse_index_json(const char *data, size_t length,
                                   source_index *index, yvex_error *err)
{
    source_json json = {data, data + length, 0u};
    char key[SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;
    size_t i;

    source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        int parsed = 1;
        source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!source_json_string(&json, key, sizeof(key))) return 0;
        source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "metadata") == 0) {
            if ((seen & 1u) || !source_parse_index_metadata(&json, index)) return 0;
            seen |= 1u;
        } else if (strcmp(key, "weight_map") == 0) {
            if (seen & 2u) return 0;
            parsed = source_parse_weight_map(&json, index, err);
            if (parsed < 0) return -1;
            if (!parsed) return 0;
            seen |= 2u;
        } else if (!source_json_skip_value(&json)) {
            return 0;
        }
        source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    if (!source_json_complete(&json) || !(seen & 2u)) return 0;
    qsort(index->items, index->count, sizeof(index->items[0]),
          source_index_compare);
    for (i = 1u; i < index->count; ++i) {
        if (strcmp(index->items[i - 1u].tensor, index->items[i].tensor) == 0) {
            index->duplicate_tensor = 1;
            return 0;
        }
    }
    return 1;
}

/* Finds one exact tensor name in the sorted caller-owned index. */
static source_index_entry *source_index_find(source_index *index,
                                             const char *tensor)
{
    size_t low = 0u;
    size_t high = index ? index->count : 0u;

    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        int cmp = strcmp(tensor, index->items[mid].tensor);
        if (cmp == 0) return &index->items[mid];
        if (cmp < 0) high = mid;
        else low = mid + 1u;
    }
    return NULL;
}

/* Copies one root shard name and size into a growable caller-owned inventory. */
static int source_shards_append(source_shards *shards, const char *name,
                                unsigned long long size, yvex_error *err)
{
    char **next_names;
    unsigned long long *next_sizes;
    size_t cap;

    if (shards->count == shards->cap) {
        cap = shards->cap ? shards->cap * 2u : 64u;
        next_names = (char **)realloc(shards->names, cap * sizeof(shards->names[0]));
        if (!next_names) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                           "shard name allocation failed");
            return YVEX_ERR_NOMEM;
        }
        shards->names = next_names;
        next_sizes = (unsigned long long *)realloc(
            shards->sizes, cap * sizeof(shards->sizes[0]));
        if (!next_sizes) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                           "shard size allocation failed");
            return YVEX_ERR_NOMEM;
        }
        shards->sizes = next_sizes;
        shards->cap = cap;
    }
    shards->names[shards->count] = source_strdup(name);
    if (!shards->names[shards->count]) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                       "shard name allocation failed");
        return YVEX_ERR_NOMEM;
    }
    shards->sizes[shards->count] = size;
    shards->count++;
    return YVEX_OK;
}

static void source_shards_free(source_shards *shards)
{
    size_t i;

    if (!shards) return;
    for (i = 0; i < shards->count; ++i) free(shards->names[i]);
    free(shards->names);
    free(shards->sizes);
    memset(shards, 0, sizeof(*shards));
}

/* Parses the exact model-NNNNN-of-NNNNN.safetensors root-shard grammar. */
static int source_shard_name_parse(const char *name,
                                   unsigned int *index_out,
                                   unsigned int *total_out)
{
    static const char prefix[] = "model-";
    static const char separator[] = "-of-";
    static const char suffix[] = ".safetensors";
    unsigned int index = 0u;
    unsigned int total = 0u;
    size_t i;

    if (!name || strlen(name) != strlen("model-00000-of-00000.safetensors") ||
        strncmp(name, prefix, sizeof(prefix) - 1u) != 0 ||
        strncmp(name + 11u, separator, sizeof(separator) - 1u) != 0 ||
        strcmp(name + 20u, suffix) != 0) return 0;
    for (i = 6u; i < 11u; ++i) {
        if (!isdigit((unsigned char)name[i])) return 0;
        index = index * 10u + (unsigned int)(name[i] - '0');
    }
    for (i = 15u; i < 20u; ++i) {
        if (!isdigit((unsigned char)name[i])) return 0;
        total = total * 10u + (unsigned int)(name[i] - '0');
    }
    if (index == 0u || total == 0u || index > total) return 0;
    if (index_out) *index_out = index;
    if (total_out) *total_out = total;
    return 1;
}

static int source_shard_name_valid(const char *name)
{
    return source_shard_name_parse(name, NULL, NULL);
}

static long source_shards_find(const source_shards *shards, const char *name)
{
    size_t i;

    if (!shards || !name) return -1;
    for (i = 0; i < shards->count; ++i) {
        if (strcmp(shards->names[i], name) == 0) return (long)i;
    }
    return -1;
}

/*
 * source_scan_root()
 *
 * Inventories regular root safetensors files, validates series geometry, and
 * accumulates checked shard bytes. It reads directory metadata only.
 */
static int source_scan_root(const char *source_path, source_shards *shards,
                            yvex_source_verification *out, yvex_error *err)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(source_path);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_scan_root",
                        "cannot inspect source directory: %s", source_path);
        return YVEX_ERR_IO;
    }
    for (;;) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        unsigned int shard_index = 0u;
        unsigned int shard_total = 0u;
        int rc;

        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno != 0) {
                source_add_blocker(out, "source-directory-read-failed");
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        if (!source_path_join(path, sizeof(path), source_path, entry->d_name)) {
            source_add_blocker(out, "source-entry-path-overflow");
            continue;
        }
        if (lstat(path, &st) != 0) {
            source_add_blocker(out, "source-entry-unreadable");
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        if (st.st_size < 0) {
            source_add_blocker(out, "invalid-source-file-size");
            continue;
        }
        if (strlen(entry->d_name) > strlen(".safetensors") &&
            strcmp(entry->d_name + strlen(entry->d_name) - strlen(".safetensors"),
                   ".safetensors") == 0) {
            size_t i;

            if (!source_shard_name_parse(entry->d_name, &shard_index,
                                         &shard_total)) {
                source_add_blocker(out, "unexpected-shard");
                continue;
            }
            if (!shards->declared_total) {
                shards->declared_total = shard_total;
            } else if (shards->declared_total != shard_total) {
                source_add_blocker(out, "inconsistent-shard-set");
            }
            for (i = 0; i < shards->count; ++i) {
                unsigned int prior_index = 0u;
                if (source_shard_name_parse(shards->names[i], &prior_index,
                                            NULL) &&
                    prior_index == shard_index) {
                    source_add_blocker(out, "duplicate-source-shard");
                }
            }
            rc = source_shards_append(shards, entry->d_name,
                                      (unsigned long long)st.st_size, err);
            if (rc != YVEX_OK) {
                closedir(dir);
                return rc;
            }
            if (!yvex_source_checked_add_u64(&out->shard_bytes,
                                             (unsigned long long)st.st_size)) {
                out->footprint_overflow = 1;
                source_add_blocker(out, "source-footprint-overflow");
            }
        }
    }
    if (closedir(dir) != 0) {
        source_add_blocker(out, "source-directory-close-failed");
    }
    out->shard_count = (unsigned long long)shards->count;
    if (!shards->count) source_add_blocker(out, "missing-source-shards");
    if (shards->declared_total &&
        shards->count != (size_t)shards->declared_total) {
        source_add_blocker(out, "incomplete-shard-set");
    }
    return YVEX_OK;
}

/*
 * source_verify_footprint()
 *
 * Reuses the source manifest scanner for recursive checked byte accounting.
 * It does not open tensor payloads; overflow becomes a typed blocker.
 */
static int source_verify_footprint(const char *source_path,
                                   yvex_source_verification *out,
                                   yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(source_path, 0, &files, err);
    if (rc == YVEX_ERR_BOUNDS) {
        out->footprint_overflow = 1;
        source_add_blocker(out, "source-footprint-overflow");
        yvex_source_manifest_file_list_free(&files);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (rc != YVEX_OK) {
        yvex_source_manifest_file_list_free(&files);
        return rc;
    }
    out->source_file_count = files.summary.file_count;
    out->source_total_bytes = files.summary.total_size_bytes;
    yvex_source_manifest_file_list_free(&files);
    return YVEX_OK;
}

static int source_is_commit(const char *text)
{
    size_t i;

    if (!text || strlen(text) != 40u) return 0;
    for (i = 0; i < 40u; ++i) {
        if (!isxdigit((unsigned char)text[i])) return 0;
    }
    return 1;
}

static int source_revision_ref_valid(const char *text)
{
    size_t i;

    if (!text || !text[0] || strcmp(text, "unknown") == 0 ||
        strcmp(text, "unverified") == 0) return 0;
    for (i = 0; text[i]; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (!isalnum(ch) && ch != '.' && ch != '_' && ch != '-' &&
            ch != '/') return 0;
    }
    return 1;
}

/* Reads one exact commit from a local Hugging Face download metadata sidecar. */
static int source_read_hf_revision(const char *source_path, const char *name,
                                   char *revision, size_t cap)
{
    char cache[YVEX_PATH_CAP];
    char path[YVEX_PATH_CAP];
    FILE *fp;
    char line[160];
    size_t length;
    int n;

    if (!source_path_join(cache, sizeof(cache), source_path,
                          ".cache/huggingface/download")) return 0;
    n = snprintf(path, sizeof(path), "%s/%s.metadata", cache, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    fp = fopen(path, "rb");
    if (!fp || !fgets(line, sizeof(line), fp)) {
        if (fp) fclose(fp);
        return 0;
    }
    fclose(fp);
    length = strcspn(line, "\r\n");
    line[length] = '\0';
    if (!source_is_commit(line) || length + 1u > cap) return 0;
    memcpy(revision, line, length + 1u);
    return 1;
}

/* Records missing or cross-file-inconsistent local source revisions. */
static void source_verify_revision_file(const char *source_path,
                                        const char *name,
                                        yvex_source_verification *out)
{
    char revision[128];

    if (!source_read_hf_revision(source_path, name, revision, sizeof(revision))) {
        source_add_blocker(out, "missing-source-revision");
        return;
    }
    if (!out->revision[0]) {
        snprintf(out->revision, sizeof(out->revision), "%s", revision);
    } else if (strcmp(out->revision, revision) != 0) {
        source_add_blocker(out, "inconsistent-source-revision");
    }
}

/* Resolves one existing manifest by explicit and canonical precedence. */
static int source_find_manifest(const yvex_source_verify_options *options,
                                char *out, size_t cap)
{
    char path[YVEX_PATH_CAP];
    int n;

    if (options->manifest_path &&
        source_regular_file(options->manifest_path, NULL)) {
        n = snprintf(out, cap, "%s", options->manifest_path);
        return n >= 0 && (size_t)n < cap;
    }
    if (source_path_join(path, sizeof(path), options->source_path,
                         "source-manifest.json") && source_regular_file(path, NULL)) {
        n = snprintf(out, cap, "%s", path);
        return n >= 0 && (size_t)n < cap;
    }
    if (source_path_join(path, sizeof(path), options->source_path,
                         "source_manifest.json") && source_regular_file(path, NULL)) {
        n = snprintf(out, cap, "%s", path);
        return n >= 0 && (size_t)n < cap;
    }
    n = options->models_root
            ? snprintf(path, sizeof(path),
                       "%s/gguf/%s/deepseek-source-manifest.json",
                       options->models_root, options->identity->family_key)
            : -1;
    if (n >= 0 && (size_t)n < sizeof(path) &&
        source_regular_file(path, NULL)) {
        n = snprintf(out, cap, "%s", path);
        return n >= 0 && (size_t)n < cap;
    }
    return 0;
}

/*
 * source_verify_manifest()
 *
 * Reads bounded structured provenance, verifies repository/source-kind/path
 * identity, and records typed blockers. Only fatal allocation failure escapes.
 */
static int source_verify_manifest(const yvex_source_verify_options *options,
                                  yvex_source_verification *out,
                                  yvex_error *err)
{
    char *data;
    size_t length;
    char manifest_local[YVEX_PATH_CAP] = "";
    char resolved_manifest_local[YVEX_PATH_CAP];

    if (!source_find_manifest(options, out->manifest_path,
                              sizeof(out->manifest_path))) {
        source_add_blocker(out, "missing-source-manifest");
        return YVEX_OK;
    }
    data = source_read_bounded_file(out->manifest_path, SOURCE_MANIFEST_CAP,
                                    &length, err);
    if (!data) {
        if (yvex_error_code(err) == YVEX_ERR_NOMEM) return YVEX_ERR_NOMEM;
        source_add_blocker(out, "malformed-source-manifest");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!source_parse_manifest(data, length, out, manifest_local,
                               sizeof(manifest_local))) {
        source_add_blocker(out, "malformed-source-manifest");
        free(data);
        return YVEX_OK;
    }
    free(data);
    if (!out->source_kind[0]) {
        source_add_blocker(out, "missing-source-kind");
    } else if (strcmp(out->source_kind, "huggingface") != 0) {
        source_add_blocker(out, "unsupported-source-kind");
    }
    if (strcmp(out->repository_id, options->identity->upstream_repo_id) != 0) {
        source_add_blocker(out, "wrong-source-repository");
    } else {
        out->repository_verified = 1;
    }
    if (!out->manifest_revision[0]) {
        source_add_blocker(out, "missing-source-revision");
    }
    if (!realpath(manifest_local, resolved_manifest_local) ||
        strcmp(resolved_manifest_local, out->resolved_source_path) != 0) {
        source_add_blocker(out, "wrong-source-local-path");
    }
    return YVEX_OK;
}

/*
 * source_verify_json_file()
 *
 * Reads and dispatches one bounded required JSON sidecar, records its exact
 * revision, and converts ordinary source defects into typed blockers.
 */
static int source_verify_json_file(const char *source_path, const char *name,
                                   size_t cap, int kind,
                                   const yvex_model_target_identity *identity,
                                   yvex_source_verification *out,
                                   yvex_error *err)
{
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    int valid = 0;

    if (!source_path_join(path, sizeof(path), source_path, name) ||
        !source_regular_file(path, NULL)) {
        source_add_blocker(
            out, kind == SOURCE_JSON_CONFIG ? "missing-source-config" :
                 kind == SOURCE_JSON_TOKENIZER ? "missing-tokenizer-json" :
                 kind == SOURCE_JSON_TOKENIZER_CONFIG
                     ? "missing-tokenizer-config"
                     : "missing-generation-config");
        return YVEX_OK;
    }
    data = source_read_bounded_file(path, cap, &length, err);
    if (!data && yvex_error_code(err) == YVEX_ERR_NOMEM) {
        return YVEX_ERR_NOMEM;
    }
    if (data) {
        if (kind == SOURCE_JSON_CONFIG) {
            valid = source_parse_config_json(data, length, identity, out);
        } else if (kind == SOURCE_JSON_TOKENIZER) {
            valid = source_parse_tokenizer_json(data, length, out);
            out->tokenizer_json_valid = valid;
        } else if (kind == SOURCE_JSON_TOKENIZER_CONFIG) {
            valid = source_parse_tokenizer_config(data, length, out);
            out->tokenizer_config_valid = valid;
        } else {
            valid = source_parse_generation_config(data, length, out);
            out->generation_config_valid = valid;
        }
        free(data);
    }
    if (!valid) {
        source_add_blocker(
            out, kind == SOURCE_JSON_CONFIG ? "malformed-source-config" :
                 kind == SOURCE_JSON_TOKENIZER ? "malformed-tokenizer-json" :
                 kind == SOURCE_JSON_TOKENIZER_CONFIG
                     ? "malformed-tokenizer-config"
                     : "malformed-generation-config");
    }
    source_verify_revision_file(source_path, name, out);
    if (!data) yvex_error_clear(err);
    return YVEX_OK;
}

/* Reads and parses the optional-on-disk but verification-required shard index. */
static int source_verify_index(const char *source_path, source_index *index,
                               yvex_source_verification *out, yvex_error *err)
{
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    int parsed;

    if (!source_path_join(path, sizeof(path), source_path,
                          "model.safetensors.index.json") ||
        !source_regular_file(path, NULL)) {
        source_add_blocker(out, "missing-shard-index");
        return YVEX_OK;
    }
    out->shard_index_present = 1;
    data = source_read_bounded_file(path, SOURCE_INDEX_CAP, &length, err);
    if (!data) {
        source_add_blocker(out, "malformed-shard-index");
        return yvex_error_code(err) == YVEX_ERR_NOMEM ? YVEX_ERR_NOMEM : YVEX_OK;
    }
    parsed = source_parse_index_json(data, length, index, err);
    free(data);
    if (parsed < 0) return yvex_error_code(err);
    if (!parsed) {
        source_add_blocker(out, index->duplicate_tensor
                                    ? "duplicate-index-tensor"
                                    : "malformed-shard-index");
        return YVEX_OK;
    }
    out->shard_index_valid = 1;
    out->indexed_tensor_count = (unsigned long long)index->count;
    source_verify_revision_file(source_path, "model.safetensors.index.json", out);
    return YVEX_OK;
}

/* Cross-checks every indexed shard reference against the root shard inventory. */
static void source_verify_index_shards(source_index *index,
                                       const source_shards *shards,
                                       yvex_source_verification *out)
{
    size_t i;
    unsigned long long unique = 0u;

    if (!out->shard_index_valid) return;
    for (i = 0; i < index->count; ++i) {
        size_t j;
        int first = 1;
        if (!source_shard_name_valid(index->items[i].shard) ||
            source_shards_find(shards, index->items[i].shard) < 0) {
            source_add_blocker(out, "missing-referenced-shard");
        }
        for (j = 0; j < i; ++j) {
            if (strcmp(index->items[j].shard, index->items[i].shard) == 0) {
                first = 0;
                break;
            }
        }
        if (first) unique++;
    }
    out->referenced_shard_count = unique;
    for (i = 0; i < shards->count; ++i) {
        size_t j;
        int found = 0;
        for (j = 0; j < index->count; ++j) {
            if (strcmp(shards->names[i], index->items[j].shard) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) source_add_blocker(out, "unexpected-shard");
    }
}

/*
 * source_verify_headers()
 *
 * Reads safetensors headers only, compares all tensor assignments with the
 * index, and accumulates raw dtype/rank/byte facts. It never reads payload
 * bytes and releases the complete temporary header table on every exit.
 */
static int source_verify_headers(const char *source_path,
                                 const source_shards *shards,
                                 source_index *index,
                                 yvex_source_verification *out,
                                 yvex_error *err)
{
    yvex_native_weight_table *table;
    size_t i;
    int mismatch = 0;

    table = (yvex_native_weight_table *)calloc(1u, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_headers",
                       "native header table allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (i = 0; i < shards->count; ++i) {
        char path[YVEX_PATH_CAP];
        yvex_error header_error;
        unsigned long long before = table->count;
        unsigned long long row;
        int rc;

        source_verify_revision_file(source_path, shards->names[i], out);
        if (!source_path_join(path, sizeof(path), source_path, shards->names[i])) {
            source_add_blocker(out, "invalid-safetensors-header");
            continue;
        }
        yvex_error_clear(&header_error);
        rc = yvex_safetensors_read_header_file(path, shards->names[i], table,
                                               &header_error);
        if (rc == YVEX_ERR_NOMEM) {
            yvex_native_weight_table_close(table);
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_headers",
                           "native header inventory allocation failed");
            return YVEX_ERR_NOMEM;
        }
        if (rc != YVEX_OK) {
            source_add_blocker(out, "invalid-safetensors-header");
            continue;
        }
        for (row = before; row < table->count && out->shard_index_valid; ++row) {
            source_index_entry *entry = source_index_find(index,
                                                          table->items[row].name);
            if (!entry || strcmp(entry->shard, shards->names[i]) != 0) {
                mismatch = 1;
            } else {
                entry->seen_in_header = 1;
            }
        }
    }
    if (out->shard_index_valid) {
        for (i = 0; i < index->count; ++i) {
            if (!index->items[i].seen_in_header) mismatch = 1;
        }
        if (mismatch || table->count != index->count) {
            source_add_blocker(out, "shard-index-header-mismatch");
        } else {
            out->shard_index_headers_match = 1;
        }
    }
    for (i = 0; i < table->count; ++i) {
        const yvex_native_weight_info *info = &table->items[i];

        if (info->rank > out->max_tensor_rank) {
            out->max_tensor_rank = info->rank;
        }
        switch (info->dtype) {
        case YVEX_NATIVE_DTYPE_F16:
            out->dtype_f16_count++;
            break;
        case YVEX_NATIVE_DTYPE_BF16:
            out->dtype_bf16_count++;
            break;
        case YVEX_NATIVE_DTYPE_F32:
            out->dtype_f32_count++;
            break;
        case YVEX_NATIVE_DTYPE_I64:
            out->dtype_i64_count++;
            break;
        case YVEX_NATIVE_DTYPE_I8:
            out->dtype_i8_count++;
            break;
        case YVEX_NATIVE_DTYPE_FP4:
            out->dtype_fp4_count++;
            break;
        case YVEX_NATIVE_DTYPE_F8_E4M3:
        case YVEX_NATIVE_DTYPE_F8_E5M2:
            out->dtype_f8_count++;
            break;
        default:
            if (strcmp(info->dtype_name, "F8_E8M0") == 0) {
                out->dtype_f8_e8m0_count++;
            } else {
                out->dtype_other_count++;
            }
            break;
        }
    }
    out->header_shard_count = table->header_read_count;
    out->header_tensor_count = table->count;
    out->header_bytes = table->header_bytes;
    out->declared_tensor_bytes = table->summary.total_tensor_bytes;
    if (index->has_declared_total_size &&
        index->declared_total_size != out->declared_tensor_bytes) {
        source_add_blocker(out, "shard-index-size-mismatch");
    }
    yvex_native_weight_table_close(table);
    return YVEX_OK;
}

const char *yvex_source_verification_status(
    const yvex_source_verification *verification)
{
    if (!verification) return "invalid";
    return verification->verified ? "verified" : "blocked";
}

/*
 * yvex_source_verify()
 *
 * Coordinates exact source verification into caller-owned facts. Ordinary
 * source defects return a blocked result; invalid arguments, allocation, or
 * internal IO failures return an error. No source file is modified.
 */
int yvex_source_verify(const yvex_source_verify_options *options,
                       yvex_source_verification *out,
                       yvex_error *err)
{
    struct stat st;
    source_shards shards;
    source_index index;
    int rc;

    if (!options || !out || !options->identity || !options->source_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_verify",
                       "identity, source path, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    memset(&shards, 0, sizeof(shards));
    memset(&index, 0, sizeof(index));
    yvex_error_clear(err);

    if (lstat(options->source_path, &st) != 0) {
        source_add_blocker(out, "missing-source-path");
        return YVEX_OK;
    }
    if (!S_ISDIR(st.st_mode) || !realpath(options->source_path,
                                         out->resolved_source_path)) {
        source_add_blocker(out, "wrong-source-path-type");
        return YVEX_OK;
    }
    out->path_verified = 1;
    rc = source_verify_manifest(options, out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_verify_json_file(options->source_path, "config.json",
                                 SOURCE_CONFIG_CAP, SOURCE_JSON_CONFIG,
                                 options->identity,
                                 out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_verify_json_file(options->source_path, "tokenizer.json",
                                 SOURCE_TOKENIZER_CAP, SOURCE_JSON_TOKENIZER,
                                 options->identity,
                                 out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_verify_json_file(options->source_path, "tokenizer_config.json",
                                 SOURCE_CONFIG_CAP, SOURCE_JSON_TOKENIZER_CONFIG,
                                 options->identity,
                                 out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_verify_json_file(options->source_path, "generation_config.json",
                                 SOURCE_CONFIG_CAP,
                                 SOURCE_JSON_GENERATION_CONFIG,
                                 options->identity, out, err);
    if (rc != YVEX_OK) goto cleanup;
    if (out->config_valid && out->generation_config_valid &&
        (out->bos_token_id != out->generation_bos_token_id ||
         out->eos_token_id != out->generation_eos_token_id)) {
        source_add_blocker(out, "generation-config-token-mismatch");
    }

    rc = source_verify_footprint(options->source_path, out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_scan_root(options->source_path, &shards, out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = source_verify_index(options->source_path, &index, out, err);
    if (rc != YVEX_OK) goto cleanup;
    source_verify_index_shards(&index, &shards, out);
    rc = source_verify_headers(options->source_path, &shards, &index, out, err);
    if (rc != YVEX_OK) goto cleanup;

    if (out->revision[0] && source_is_commit(out->revision) &&
        source_revision_ref_valid(out->manifest_revision) &&
        !source_has_blocker(out, "missing-source-revision") &&
        !source_has_blocker(out, "inconsistent-source-revision") &&
        (!source_is_commit(out->manifest_revision) ||
         strcmp(out->manifest_revision, out->revision) == 0)) {
        out->revision_verified = 1;
    } else if (out->manifest_revision[0] && source_is_commit(out->manifest_revision) &&
               out->revision[0] &&
               strcmp(out->manifest_revision, out->revision) != 0) {
        source_add_blocker(out, "inconsistent-source-revision");
    } else if (!out->revision[0] || !out->manifest_revision[0]) {
        source_add_blocker(out, "missing-source-revision");
    } else if (!source_has_blocker(out, "missing-source-revision") &&
               !source_has_blocker(out, "inconsistent-source-revision")) {
        source_add_blocker(out, "unverifiable-source-revision");
    }
    if (out->manifest_status[0] &&
        strcmp(out->manifest_status, "complete") != 0) {
        source_add_blocker(out, "source-manifest-incomplete");
    }
    out->verified = out->blocker_count == 0u && out->path_verified &&
                    out->repository_verified && out->revision_verified &&
                    out->config_valid && out->tokenizer_json_valid &&
                    out->tokenizer_config_valid &&
                    out->generation_config_valid && out->shard_index_valid &&
                    out->shard_index_headers_match;
cleanup:
    source_index_free(&index);
    source_shards_free(&shards);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}
