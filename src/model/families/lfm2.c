/* Source interpretation, not an execution shortcut. Equations are referenced
 * to Transformers ddb849abe009d1089e6c691bfc897f27211c663c (v5.12.1).
 * The original BF16 checkpoint is not the separately distilled QAD checkpoint. */
#include <yvex/internal/families/lfm2.h>
#include <yvex/internal/core.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int lfm_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "lfm2.source", reason);
    return status;
}

typedef struct { const char *key; size_t offset; int real; } lfm_field;
static const lfm_field lfm_fields[] = {
    {"hidden_size", offsetof(yvex_lfm2_architecture, hidden), 0},
    {"num_hidden_layers", offsetof(yvex_lfm2_architecture, layers), 0},
    {"vocab_size", offsetof(yvex_lfm2_architecture, vocabulary), 0},
    {"intermediate_size", offsetof(yvex_lfm2_architecture, intermediate), 0},
    {"moe_intermediate_size", offsetof(yvex_lfm2_architecture, expert_intermediate), 0},
    {"num_attention_heads", offsetof(yvex_lfm2_architecture, heads), 0},
    {"num_key_value_heads", offsetof(yvex_lfm2_architecture, kv_heads), 0},
    {"num_experts", offsetof(yvex_lfm2_architecture, experts), 0},
    {"num_experts_per_tok", offsetof(yvex_lfm2_architecture, selected_experts), 0},
    {"num_dense_layers", offsetof(yvex_lfm2_architecture, dense_layers), 0},
    {"max_position_embeddings", offsetof(yvex_lfm2_architecture, context), 0},
    {"conv_L_cache", offsetof(yvex_lfm2_architecture, convolution_kernel), 0},
    {"bos_token_id", offsetof(yvex_lfm2_architecture, bos), 0},
    {"eos_token_id", offsetof(yvex_lfm2_architecture, eos), 0},
    {"pad_token_id", offsetof(yvex_lfm2_architecture, pad), 0},
    {"norm_eps", offsetof(yvex_lfm2_architecture, norm_epsilon), 1},
    {"routed_scaling_factor", offsetof(yvex_lfm2_architecture, routed_scale), 1},
};

static int lfm_text(yvex_json *json, const char *expected)
{
    char value[128];
    return yvex_json_string(json, value, sizeof(value)) && !strcmp(value, expected);
}

static int lfm_real(yvex_json *json, double *out)
{
    char text[64], *end;
    if (!yvex_json_number_text(json, text, sizeof(text))) return 0;
    errno = 0;
    *out = strtod(text, &end);
    return !errno && !*end && isfinite(*out);
}

static int lfm_layers(yvex_json *json, yvex_lfm2_architecture *a, size_t *count)
{
    yvex_json_iter array;
    yvex_json_item item;
    char value[32];
    if (!yvex_json_iter_begin(json, &array, YVEX_JSON_COLLECTION_ARRAY)) return 0;
    while ((item = yvex_json_array_value(&array)) == YVEX_JSON_ITEM_READY) {
        if (*count == YVEX_LFM2_LAYER_CAP || !yvex_json_string(json, value, sizeof(value))) return 0;
        if (!strcmp(value, "conv")) {
            a->mixer[(*count)++] = YVEX_LFM2_MIXER_CONV;
            a->convolution_layers++;
        } else if (!strcmp(value, "full_attention")) {
            a->mixer[(*count)++] = YVEX_LFM2_MIXER_ATTENTION;
            a->attention_layers++;
        } else return 0;
    }
    return item == YVEX_JSON_ITEM_END && !array.trailing_separator;
}

static int lfm_rope(yvex_json *json, double *theta)
{
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP];
    unsigned int seen = 0u;
    if (!yvex_json_iter_begin(json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        unsigned int bit = !strcmp(key, "rope_theta") ? 1u : !strcmp(key, "rope_type") ? 2u : 0u;
        if (!bit || (seen & bit)) return 0;
        seen |= bit;
        if (bit == 1u ? !lfm_real(json, theta) : !lfm_text(json, "default")) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator && seen == 3u &&
        isfinite(*theta) && *theta > 1.0;
}

static int lfm_config_seal(yvex_lfm2_architecture *a)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    if (!a->hidden || a->hidden > 65536u || !a->layers || a->layers > YVEX_LFM2_LAYER_CAP ||
        !a->heads || !a->kv_heads || a->heads % a->kv_heads || a->hidden % a->heads ||
        !a->vocabulary || a->vocabulary > 1048576u || !a->intermediate || a->intermediate > 1048576u ||
        !a->expert_intermediate || a->expert_intermediate > 1048576u ||
        !a->experts || a->experts > YVEX_LFM2_EXPERT_CAP || !a->selected_experts ||
        a->selected_experts > a->experts || a->dense_layers > a->layers ||
        !a->context || a->context > 1048576u || a->convolution_kernel < 2u ||
        a->convolution_kernel > 64u || a->bos >= a->vocabulary || a->eos >= a->vocabulary ||
        a->pad >= a->vocabulary || a->eos == a->bos || a->pad == a->eos ||
        !isfinite(a->norm_epsilon) || a->norm_epsilon <= 0.0 ||
        !isfinite(a->routed_scale) || a->routed_scale <= 0.0) return 0;
    a->head_width = a->hidden / a->heads;
    if (a->head_width % 2u) return 0;
    /* Tied embedding/head: two globals, not a fabricated lm_head tensor. */
    a->required_tensors = 2u;
    for (size_t i = 0u; i < a->layers; ++i)
        a->required_tensors += 2u + (a->mixer[i] == YVEX_LFM2_MIXER_CONV ? 3u : 6u) +
            (i < a->dense_layers ? 3u : 2u + 3u * a->experts);
    yvex_sha256_init(&hash);
    yvex_sha256_update_text(&hash, "yvex.lfm2.source-topology.v1");
    yvex_sha256_update_text(&hash, YVEX_LFM2_REPOSITORY);
    yvex_sha256_update_text(&hash, YVEX_LFM2_REVISION);
    for (size_t i = 0u; i < sizeof(lfm_fields) / sizeof(lfm_fields[0]); ++i) {
        unsigned long long bits;
        memcpy(&bits, (const unsigned char *)a + lfm_fields[i].offset, sizeof(bits));
        yvex_sha256_update_text(&hash, lfm_fields[i].key);
        yvex_sha256_update_u64(&hash, bits);
    }
    {
        unsigned long long bits;
        memcpy(&bits, &a->rope_theta, sizeof(bits));
        yvex_sha256_update_u64(&hash, bits);
    }
    for (size_t i = 0u; i < a->layers; ++i) yvex_sha256_update_u64(&hash, a->mixer[i]);
    yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest, a->identity);
    a->complete = 1;
    return 1;
}

static int lfm_config_parse(const char *data, size_t length, yvex_lfm2_architecture *out, yvex_error *err)
{
    static const char *const extra[] = {"model_type", "architectures", "dtype", "layer_types",
        "rope_parameters", "conv_bias", "norm_topk_prob", "use_expert_bias", "tie_word_embeddings",
        "use_cache", "initializer_range", "transformers_version"};
    yvex_lfm2_architecture a = {0};
    yvex_json json;
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP], ignored[64];
    unsigned long long seen = 0u;
    size_t count = 0u, field_count = sizeof(lfm_fields) / sizeof(lfm_fields[0]);
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !data || !length)
        return lfm_refuse(err, YVEX_ERR_INVALID_ARG, "configuration and output are required");
    yvex_json_init(&json, data, length);
    if (!yvex_json_iter_begin(&json, &object, YVEX_JSON_COLLECTION_OBJECT)) goto invalid;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        size_t index;
        for (index = 0u; index < field_count && strcmp(key, lfm_fields[index].key); ++index) {}
        if (index < field_count) {
            void *value = (unsigned char *)&a + lfm_fields[index].offset;
            if ((seen & (1ull << index)) || (lfm_fields[index].real ?
                !lfm_real(&json, value) : !yvex_json_u64(&json, value))) goto invalid;
        } else {
            size_t e;
            int value;
            double initializer;
            for (e = 0u; e < sizeof(extra) / sizeof(extra[0]) && strcmp(key, extra[e]); ++e) {}
            if (e == sizeof(extra) / sizeof(extra[0])) goto invalid;
            index += e;
            if (seen & (1ull << index)) goto invalid;
            switch (e) {
            case 0u: if (!lfm_text(&json, "lfm2_moe")) goto invalid; break;
            case 1u: {
                yvex_json_iter array;
                if (!yvex_json_iter_begin(&json, &array, YVEX_JSON_COLLECTION_ARRAY) ||
                    yvex_json_array_value(&array) != YVEX_JSON_ITEM_READY ||
                    !lfm_text(&json, "Lfm2MoeForCausalLM") ||
                    yvex_json_array_value(&array) != YVEX_JSON_ITEM_END || array.trailing_separator) goto invalid;
                break;
            }
            case 2u: if (!lfm_text(&json, "bfloat16")) goto invalid; break;
            case 3u: if (!lfm_layers(&json, &a, &count)) goto invalid; break;
            case 4u: if (!lfm_rope(&json, &a.rope_theta)) goto invalid; break;
            case 5u: case 6u: case 7u: case 8u: case 9u:
                if (!yvex_json_bool(&json, &value) || value != (e != 5u)) goto invalid;
                break;
            case 10u: if (!lfm_real(&json, &initializer)) goto invalid; break;
            case 11u: if (!yvex_json_string(&json, ignored, sizeof(ignored))) goto invalid; break;
            default: goto invalid;
            }
        }
        seen |= 1ull << index;
    }
    /* Initializer/version are descriptive, all computational facts mandatory. */
    if (item != YVEX_JSON_ITEM_END || object.trailing_separator || !yvex_json_complete(&json) ||
        (seen & ((1ull << (field_count + 10u)) - 1u)) != ((1ull << (field_count + 10u)) - 1u) ||
        count != a.layers || !lfm_config_seal(&a)) goto invalid;
    *out = a;
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    return lfm_refuse(err, YVEX_ERR_FORMAT, "unsupported or inconsistent LFM2 configuration");
}

static int lfm_open(const yvex_source_verification *v, yvex_lfm2_architecture *a, yvex_error *err)
{
    yvex_source_metadata_blob blob = {0};
    int rc;
    if (a) memset(a, 0, sizeof(*a));
    if (!a || !v || !v->verified || !v->config_valid || v->blocker_count ||
        strcmp(v->repository_id, YVEX_LFM2_REPOSITORY) || strcmp(v->revision, YVEX_LFM2_REVISION))
        return lfm_refuse(err, YVEX_ERR_FORMAT, "exact verified original LFM2.5 checkpoint is required");
    rc = yvex_source_provenance_metadata_read(v, "config.json", 65536u, &blob, err);
    if (rc == YVEX_OK) rc = lfm_config_parse((const char *)blob.bytes, blob.byte_count, a, err);
    yvex_source_metadata_blob_release(&blob);
    /* Source pads tied embedding/output rows beyond the tokenizer's published
     * IDs. Preserve both populations; never fabricate additional token strings. */
    if (rc == YVEX_OK && (!v->tokenizer_effective_vocab_size ||
        v->tokenizer_effective_vocab_size > a->vocabulary ||
        a->bos >= v->tokenizer_effective_vocab_size || a->eos >= v->tokenizer_effective_vocab_size ||
        a->pad >= v->tokenizer_effective_vocab_size)) {
        memset(a, 0, sizeof(*a));
        return lfm_refuse(err, YVEX_ERR_FORMAT, "tokenizer vocabulary and model geometry disagree");
    }
    return rc;
}

static int lfm_ordinal(const char **cursor, unsigned long long *out)
{
    char *end;
    const char *s = *cursor;
    if (*s < '0' || *s > '9' || (*s == '0' && s[1] != '.')) return 0;
    errno = 0;
    *out = strtoull(s, &end, 10);
    if (errno || *end != '.') return 0;
    *cursor = end + 1u;
    return 1;
}

static yvex_lfm2_role lfm_role(const char *name, yvex_lfm2_tensor_binding *b)
{
    static const struct { const char *suffix; yvex_lfm2_role role; } roles[] = {
        {"operator_norm.weight", YVEX_LFM2_ROLE_OPERATOR_NORM}, {"ffn_norm.weight", YVEX_LFM2_ROLE_FFN_NORM},
        {"conv.conv.weight", YVEX_LFM2_ROLE_CONV_WEIGHT}, {"conv.in_proj.weight", YVEX_LFM2_ROLE_CONV_IN},
        {"conv.out_proj.weight", YVEX_LFM2_ROLE_CONV_OUT}, {"self_attn.q_proj.weight", YVEX_LFM2_ROLE_QUERY},
        {"self_attn.k_proj.weight", YVEX_LFM2_ROLE_KEY}, {"self_attn.v_proj.weight", YVEX_LFM2_ROLE_VALUE},
        {"self_attn.out_proj.weight", YVEX_LFM2_ROLE_ATTENTION_OUT},
        {"self_attn.q_layernorm.weight", YVEX_LFM2_ROLE_QUERY_NORM},
        {"self_attn.k_layernorm.weight", YVEX_LFM2_ROLE_KEY_NORM},
        {"feed_forward.w1.weight", YVEX_LFM2_ROLE_DENSE_GATE},
        {"feed_forward.w3.weight", YVEX_LFM2_ROLE_DENSE_UP},
        {"feed_forward.w2.weight", YVEX_LFM2_ROLE_DENSE_DOWN},
        {"feed_forward.gate.weight", YVEX_LFM2_ROLE_ROUTER},
        {"feed_forward.expert_bias", YVEX_LFM2_ROLE_ROUTER_BIAS},
    };
    b->layer = b->expert = ~0ull;
    if (!strcmp(name, "model.embed_tokens.weight")) return YVEX_LFM2_ROLE_EMBEDDING;
    if (!strcmp(name, "model.embedding_norm.weight")) return YVEX_LFM2_ROLE_FINAL_NORM;
    if (strncmp(name, "model.layers.", 13u)) return YVEX_LFM2_ROLE_UNKNOWN;
    name += 13u;
    if (!lfm_ordinal(&name, &b->layer)) return YVEX_LFM2_ROLE_UNKNOWN;
    for (size_t i = 0u; i < sizeof(roles) / sizeof(roles[0]); ++i)
        if (!strcmp(name, roles[i].suffix)) return roles[i].role;
    if (strncmp(name, "feed_forward.experts.", 21u)) return YVEX_LFM2_ROLE_UNKNOWN;
    name += 21u;
    if (!lfm_ordinal(&name, &b->expert)) return YVEX_LFM2_ROLE_UNKNOWN;
    return !strcmp(name, "w1.weight") ? YVEX_LFM2_ROLE_EXPERT_GATE :
        !strcmp(name, "w3.weight") ? YVEX_LFM2_ROLE_EXPERT_UP :
        !strcmp(name, "w2.weight") ? YVEX_LFM2_ROLE_EXPERT_DOWN : YVEX_LFM2_ROLE_UNKNOWN;
}

static int lfm_classify(const yvex_lfm2_architecture *a, const yvex_native_weight_info *t,
                        yvex_lfm2_tensor_binding *out, yvex_error *err)
{
    yvex_lfm2_tensor_binding b = {0};
    unsigned long long shape[3] = {0}, elements = 1u, bytes;
    unsigned int rank = 1u;
    yvex_native_dtype dtype = YVEX_NATIVE_DTYPE_BF16;
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !a || !a->complete || !t || !t->name || a->layers > YVEX_LFM2_LAYER_CAP)
        return lfm_refuse(err, YVEX_ERR_INVALID_ARG, "sealed architecture and tensor required");
    b.role = lfm_role(t->name, &b);
    if (b.role == YVEX_LFM2_ROLE_UNKNOWN || (b.layer != ~0ull && b.layer >= a->layers) ||
        (b.expert != ~0ull && b.expert >= a->experts)) goto invalid;
    if (b.role >= YVEX_LFM2_ROLE_CONV_WEIGHT && b.role <= YVEX_LFM2_ROLE_CONV_OUT &&
        a->mixer[b.layer] != YVEX_LFM2_MIXER_CONV) goto invalid;
    if (b.role >= YVEX_LFM2_ROLE_QUERY && b.role <= YVEX_LFM2_ROLE_KEY_NORM &&
        a->mixer[b.layer] != YVEX_LFM2_MIXER_ATTENTION) goto invalid;
    if (b.role >= YVEX_LFM2_ROLE_DENSE_GATE && b.role <= YVEX_LFM2_ROLE_DENSE_DOWN &&
        b.layer >= a->dense_layers) goto invalid;
    if (b.role >= YVEX_LFM2_ROLE_ROUTER && b.layer < a->dense_layers) goto invalid;
    shape[0] = a->hidden;
    switch (b.role) {
    case YVEX_LFM2_ROLE_EMBEDDING: shape[0] = a->vocabulary; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_CONV_WEIGHT: shape[1] = 1u; shape[2] = a->convolution_kernel; rank = 3u; break;
    case YVEX_LFM2_ROLE_CONV_IN: shape[0] = 3u * a->hidden; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_CONV_OUT: case YVEX_LFM2_ROLE_QUERY: case YVEX_LFM2_ROLE_ATTENTION_OUT:
        shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_KEY: case YVEX_LFM2_ROLE_VALUE:
        shape[0] = a->kv_heads * a->head_width; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_QUERY_NORM: case YVEX_LFM2_ROLE_KEY_NORM: shape[0] = a->head_width; break;
    case YVEX_LFM2_ROLE_DENSE_GATE: case YVEX_LFM2_ROLE_DENSE_UP:
        shape[0] = a->intermediate; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_DENSE_DOWN: shape[1] = a->intermediate; rank = 2u; break;
    case YVEX_LFM2_ROLE_ROUTER: shape[0] = a->experts; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_ROUTER_BIAS: shape[0] = a->experts; dtype = YVEX_NATIVE_DTYPE_F32; break;
    case YVEX_LFM2_ROLE_EXPERT_GATE: case YVEX_LFM2_ROLE_EXPERT_UP:
        shape[0] = a->expert_intermediate; shape[1] = a->hidden; rank = 2u; break;
    case YVEX_LFM2_ROLE_EXPERT_DOWN: shape[1] = a->expert_intermediate; rank = 2u; break;
    default: break;
    }
    if (t->rank != rank || t->dtype != dtype) goto invalid;
    for (unsigned int i = 0u; i < rank; ++i)
        if (t->dims[i] != shape[i] || !yvex_core_u64_mul(elements, shape[i], &elements)) goto invalid;
    if (!yvex_core_u64_mul(elements, dtype == YVEX_NATIVE_DTYPE_F32 ? 4u : 2u, &bytes) ||
        t->data_bytes != bytes || t->data_end < t->data_start || t->data_end - t->data_start != bytes)
        goto invalid;
    *out = b;
    return YVEX_OK;
invalid:
    return lfm_refuse(err, YVEX_ERR_FORMAT, "LFM2 tensor role, topology, dtype or geometry mismatch");
}

static int lfm_audit(const yvex_lfm2_architecture *a, const yvex_native_weight_table *table,
                     const yvex_source_tensor_snapshot *snapshot, yvex_lfm2_inventory *out, yvex_error *err)
{
    yvex_source_tensor_snapshot_facts facts;
    yvex_lfm2_inventory inventory = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES], *seen;
    unsigned long long count, slots;
    int rc = YVEX_OK;
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !a || !a->complete || (!table && !snapshot) || a->layers > YVEX_LFM2_LAYER_CAP ||
        !a->experts || a->experts > YVEX_LFM2_EXPERT_CAP)
        return lfm_refuse(err, YVEX_ERR_INVALID_ARG, "sealed architecture and inventory required");
    if (snapshot && yvex_source_tensor_snapshot_facts_get(snapshot, &facts, err) != YVEX_OK)
        return yvex_error_code(err);
    count = snapshot ? facts.tensor_count : yvex_native_weight_table_count(table);
    if (count != a->required_tensors)
        return lfm_refuse(err, YVEX_ERR_FORMAT, "incomplete or unexpected LFM2 tensor population");
    slots = (a->layers + 1u) * (a->experts + 1u) * YVEX_LFM2_ROLE_COUNT;
    seen = calloc((size_t)slots, 1u);
    if (!seen) return lfm_refuse(err, YVEX_ERR_NOMEM, "role coverage allocation failed");
    for (unsigned long long i = 0u; i < count && rc == YVEX_OK; ++i) {
        const yvex_native_weight_info *t = snapshot ? yvex_source_tensor_snapshot_at(snapshot, i) :
            yvex_native_weight_table_at(table, i);
        yvex_lfm2_tensor_binding b;
        unsigned long long slot;
        rc = lfm_classify(a, t, &b, err);
        if (rc != YVEX_OK) break;
        slot = ((b.layer == ~0ull ? a->layers : b.layer) * (a->experts + 1u) +
            (b.expert == ~0ull ? a->experts : b.expert)) * YVEX_LFM2_ROLE_COUNT + b.role;
        if (seen[slot]++ || !yvex_core_u64_add(inventory.bytes, t->data_bytes, &inventory.bytes))
            rc = lfm_refuse(err, YVEX_ERR_FORMAT, "duplicate role or overflowing LFM2 inventory");
        inventory.bf16_tensors += t->dtype == YVEX_NATIVE_DTYPE_BF16;
        inventory.f32_tensors += t->dtype == YVEX_NATIVE_DTYPE_F32;
    }
    /* Every member is legal and unique; equality with the exact legal population
     * proves coverage without imposing shard/header enumeration order. */
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        yvex_sha256_update_text(&hash, "yvex.lfm2.role-coverage.v1");
        yvex_sha256_update_text(&hash, a->identity);
        for (unsigned long long i = 0u; i < slots; ++i) yvex_sha256_update_u64(&hash, seen[i]);
        yvex_sha256_final(&hash, digest);
        yvex_sha256_hex(digest, inventory.identity);
        inventory.tensors = count;
        inventory.complete = 1;
        *out = inventory;
        yvex_error_clear(err);
    }
    free(seen);
    return rc;
}
static int lfm_table_audit(const yvex_lfm2_architecture *a, const yvex_native_weight_table *t,
                           yvex_lfm2_inventory *out, yvex_error *err)
{
    return lfm_audit(a, t, NULL, out, err);
}
static int lfm_snapshot_audit(const yvex_lfm2_architecture *a, const yvex_source_tensor_snapshot *s,
                              yvex_lfm2_inventory *out, yvex_error *err)
{
    return lfm_audit(a, NULL, s, out, err);
}
typedef struct {
    yvex_ir_module *module;
    const yvex_lfm2_architecture *a;
    const char *source;
    yvex_ir_id block, sequence, hidden, convolution, kv, position;
    char prefix[64];
    int rc;
    yvex_error *error;
} lfm_program;

static yvex_ir_id lfm_type(lfm_program *b, yvex_ir_type_kind kind, yvex_ir_scalar scalar,
    const uint64_t *shape, uint32_t rank, const char *domain, int rows)
{
    yvex_ir_type t = {.kind = kind, .scalar = scalar, .rank = rank};
    yvex_ir_id id = YVEX_IR_NONE;
    for (uint32_t i = 0u; i < rank; ++i) t.shape[i] = (yvex_ir_extent){YVEX_IR_NONE, shape[i]};
    if (rows) t.shape[0] = (yvex_ir_extent){b->sequence, 0u};
    if (domain) yvex_core_text_copy(t.domain, sizeof(t.domain), domain);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->error);
    return id;
}

static yvex_ir_id lfm_rows(lfm_program *b, uint64_t width, yvex_ir_scalar scalar)
{
    uint64_t shape[] = {1u, width};
    return lfm_type(b, YVEX_IR_TENSOR, scalar, shape, 2u, NULL, 1);
}

static yvex_ir_id lfm_emit(lfm_program *b, const char *name, uint32_t version,
    const yvex_ir_id *args, size_t inputs, const yvex_ir_id *types, size_t outputs,
    const yvex_ir_attribute *attrs, size_t count)
{
    yvex_ir_id op = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .version = version,
        .operands = args, .operand_count = inputs, .result_types = types, .result_count = outputs,
        .attributes = attrs, .attribute_count = count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &op, b->error);
    return op;
}

static yvex_ir_id lfm_result(lfm_program *b, yvex_ir_id op, uint32_t ordinal)
{
    const yvex_ir_operation *operation = yvex_ir_operation_at(b->module, op);
    return operation && ordinal < operation->result_count ? operation->results[ordinal] : YVEX_IR_NONE;
}

static yvex_ir_id lfm_parameter(lfm_program *b, const char *suffix, const uint64_t *shape,
    uint32_t rank, yvex_ir_scalar scalar, int bank)
{
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT},
        {.name = "expert_count", .kind = YVEX_IR_ATTR_U64, .value.integer = b->a->experts}};
    yvex_ir_id type = lfm_type(b, YVEX_IR_TENSOR, scalar, shape, rank, NULL, 0);
    (void)snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "%s%s", b->prefix, suffix);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->source);
    return lfm_result(b, lfm_emit(b, "core.parameter", bank ? 2u : 1u, NULL, 0u, &type, 1u,
        attrs, bank ? 3u : 2u), 0u);
}

static yvex_ir_id lfm_linear(lfm_program *b, yvex_ir_id input, const char *name,
    uint64_t columns, uint64_t rows)
{
    uint64_t shape[] = {rows, columns};
    yvex_ir_id args[] = {input, lfm_parameter(b, name, shape, 2u, YVEX_IR_BF16, 0)};
    yvex_ir_id type = lfm_rows(b, rows, YVEX_IR_BF16);
    return lfm_result(b, lfm_emit(b, "nn.linear", 1u, args, 2u, &type, 1u, NULL, 0u), 0u);
}

static yvex_ir_id lfm_norm(lfm_program *b, yvex_ir_id input, const char *name,
    uint64_t width, uint64_t group)
{
    uint64_t shape[] = {group};
    yvex_ir_id args[] = {input, lfm_parameter(b, name, shape, 1u, YVEX_IR_BF16, 0)};
    yvex_ir_id type = lfm_rows(b, width, YVEX_IR_BF16);
    yvex_ir_attribute attrs[] = {
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = b->a->norm_epsilon},
        {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64, .value.real = 0.0}};
    return lfm_result(b, lfm_emit(b, group == width ? "nn.rms_norm" : "nn.group_rms_norm",
        2u, args, 2u, &type, 1u, attrs, group == width ? 2u : 1u), 0u);
}

static yvex_ir_id lfm_mixer(lfm_program *b, yvex_ir_id input, yvex_ir_id start,
    yvex_ir_id *state, yvex_lfm2_mixer mixer)
{
    const yvex_lfm2_architecture *a = b->a;
    yvex_ir_id op, types[] = {b->hidden, mixer == YVEX_LFM2_MIXER_CONV ? b->convolution : b->kv};
    if (mixer == YVEX_LFM2_MIXER_CONV) {
        uint64_t weight[] = {a->hidden, 1u, a->convolution_kernel};
        yvex_ir_id args[] = {lfm_linear(b, input, "conv.in_proj.weight", a->hidden, 3u * a->hidden),
            lfm_parameter(b, "conv.conv.weight", weight, 3u, YVEX_IR_BF16, 0), *state};
        yvex_ir_attribute attrs[] = {
            {.name = "width", .kind = YVEX_IR_ATTR_U64, .value.integer = a->hidden},
            {.name = "convolution_kernel", .kind = YVEX_IR_ATTR_U64, .value.integer = a->convolution_kernel}};
        op = lfm_emit(b, "sequence.double_gated_conv", 1u, args, 3u, types, 2u, attrs, 2u);
    } else {
        uint64_t kv = a->kv_heads * a->head_width;
        yvex_ir_id q = lfm_linear(b, input, "self_attn.q_proj.weight", a->hidden, a->hidden);
        yvex_ir_id k = lfm_linear(b, input, "self_attn.k_proj.weight", a->hidden, kv);
        yvex_ir_id v = lfm_linear(b, input, "self_attn.v_proj.weight", a->hidden, kv);
        q = lfm_norm(b, q, "self_attn.q_layernorm.weight", a->hidden, a->head_width);
        k = lfm_norm(b, k, "self_attn.k_layernorm.weight", kv, a->head_width);
        yvex_ir_id args[] = {q, k, v, start, *state};
        yvex_ir_attribute attrs[] = {
            {.name = "query_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = a->heads},
            {.name = "kv_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = a->kv_heads},
            {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = a->head_width},
            {.name = "rotary_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = a->head_width},
            {.name = "maximum_context", .kind = YVEX_IR_ATTR_U64, .value.integer = a->context},
            {.name = "theta", .kind = YVEX_IR_ATTR_U64, .value.integer = (uint64_t)a->rope_theta}};
        op = lfm_emit(b, "attention.causal", 1u, args, 5u, types, 2u, attrs, 6u);
    }
    *state = lfm_result(b, op, 1u);
    return lfm_linear(b, lfm_result(b, op, 0u), mixer == YVEX_LFM2_MIXER_CONV ?
        "conv.out_proj.weight" : "self_attn.out_proj.weight", a->hidden, a->hidden);
}

static yvex_ir_id lfm_ffn(lfm_program *b, yvex_ir_id input, int dense)
{
    const yvex_lfm2_architecture *a = b->a;
    if (dense) {
        yvex_ir_id gate = lfm_linear(b, input, "feed_forward.w1.weight", a->hidden, a->intermediate);
        yvex_ir_id up = lfm_linear(b, input, "feed_forward.w3.weight", a->hidden, a->intermediate);
        yvex_ir_id args[] = {gate, up}, type = lfm_rows(b, a->intermediate, YVEX_IR_BF16);
        yvex_ir_id activation = lfm_result(b, lfm_emit(b, "nn.silu_product", 1u,
            args, 2u, &type, 1u, NULL, 0u), 0u);
        return lfm_linear(b, activation, "feed_forward.w2.weight", a->intermediate, a->hidden);
    }
    uint64_t gate_shape[] = {a->experts, a->expert_intermediate, a->hidden};
    uint64_t down_shape[] = {a->experts, a->hidden, a->expert_intermediate}, bias_shape[] = {a->experts};
    yvex_ir_id args[] = {input, lfm_linear(b, input, "feed_forward.gate.weight", a->hidden, a->experts),
        lfm_parameter(b, "feed_forward.expert_bias", bias_shape, 1u, YVEX_IR_F32, 0),
        lfm_parameter(b, "feed_forward.experts.0.w1.weight", gate_shape, 3u, YVEX_IR_BF16, 1),
        lfm_parameter(b, "feed_forward.experts.0.w3.weight", gate_shape, 3u, YVEX_IR_BF16, 1),
        lfm_parameter(b, "feed_forward.experts.0.w2.weight", down_shape, 3u, YVEX_IR_BF16, 1)};
    yvex_ir_attribute attrs[] = {
        {.name = "selected_experts", .kind = YVEX_IR_ATTR_U64, .value.integer = a->selected_experts},
        {.name = "normalization_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-6},
        {.name = "routed_scale", .kind = YVEX_IR_ATTR_F64, .value.real = a->routed_scale}};
    return lfm_result(b, lfm_emit(b, "nn.routed_swiglu", 1u, args, 6u, &b->hidden, 1u, attrs, 3u), 0u);
}

static yvex_ir_id lfm_add(lfm_program *b, yvex_ir_id x, yvex_ir_id y)
{
    yvex_ir_id args[] = {x, y};
    return lfm_result(b, lfm_emit(b, "tensor.add", 1u, args, 2u, &b->hidden, 1u, NULL, 0u), 0u);
}

static int lfm_functions(lfm_program *b)
{
    const yvex_lfm2_architecture *a = b->a;
    yvex_ir_id args[YVEX_LFM2_LAYER_CAP + 2u], types[YVEX_LFM2_LAYER_CAP + 1u];
    yvex_ir_id results[YVEX_LFM2_LAYER_CAP + 1u], function, input, start;
    uint64_t token_shape[] = {1u}, embedding[] = {a->vocabulary, a->hidden};
    args[0] = lfm_type(b, YVEX_IR_TENSOR, YVEX_IR_INDEX, token_shape, 1u, NULL, 1);
    args[1] = b->position; types[0] = b->hidden;
    for (uint64_t l = 0u; l < a->layers; ++l)
        args[l + 2u] = types[l + 1u] = a->mixer[l] == YVEX_LFM2_MIXER_CONV ? b->convolution : b->kv;
    if (b->rc == YVEX_OK)
        b->rc = yvex_ir_function_add(b->module, "forward", args, a->layers + 2u,
            types, a->layers + 1u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, &function, b->error);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, (a->layers + 2u) * sizeof(*args));
    start = args[1];
    memcpy(results + 1u, args + 2u, a->layers * sizeof(*args));
    args[1] = lfm_parameter(b, "model.embed_tokens.weight", embedding, 2u, YVEX_IR_BF16, 0);
    input = lfm_result(b, lfm_emit(b, "nn.embedding", 1u, args, 2u, &b->hidden, 1u, NULL, 0u), 0u);
    for (uint64_t l = 0u; b->rc == YVEX_OK && l < a->layers; ++l) {
        (void)snprintf(b->prefix, sizeof(b->prefix), "model.layers.%llu.", (unsigned long long)l);
        yvex_ir_id norm = lfm_norm(b, input, "operator_norm.weight", a->hidden, a->hidden);
        yvex_ir_id update = lfm_mixer(b, norm, start, &results[l + 1u], a->mixer[l]);
        input = lfm_add(b, input, update);
        norm = lfm_norm(b, input, "ffn_norm.weight", a->hidden, a->hidden);
        update = lfm_ffn(b, norm, l < a->dense_layers);
        input = lfm_add(b, input, update);
    }
    b->prefix[0] = '\0';
    results[0] = lfm_norm(b, input, "model.embedding_norm.weight", a->hidden, a->hidden);
    (void)lfm_emit(b, "core.return", 1u, results, a->layers + 1u, NULL, 0u, NULL, 0u);
    types[0] = lfm_rows(b, a->vocabulary, YVEX_IR_F32);
    if (b->rc == YVEX_OK)
        b->rc = yvex_ir_function_add(b->module, "output", &b->hidden, 1u, types, 1u, 0u, &function, b->error);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    input = yvex_ir_block_at(b->module, b->block)->arguments[0];
    input = lfm_linear(b, input, "model.embed_tokens.weight", a->hidden, a->vocabulary);
    results[0] = lfm_result(b, lfm_emit(b, "tensor.cast", 1u, &input, 1u, types, 1u, NULL, 0u), 0u);
    (void)lfm_emit(b, "core.return", 1u, results, 1u, NULL, 0u, NULL, 0u);
    return b->rc;
}

static int lfm_program_build(yvex_ir_module **out, const yvex_lfm2_architecture *a,
    const char *source, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *yvex_ir_sequence_dialect()};
    yvex_lfm2_architecture checked;
    lfm_program b = {.a = a, .source = source, .error = err};
    if (out) *out = NULL;
    if (!out || !a || !a->complete || !yvex_sha256_hex_valid(source))
        return lfm_refuse(err, YVEX_ERR_INVALID_ARG, "complete source architecture and payload identity required");
    checked = *a;
    if (!lfm_config_seal(&checked) || strcmp(checked.identity, a->identity) ||
        floor(a->rope_theta) != a->rope_theta || a->rope_theta >= (double)UINT64_MAX)
        return lfm_refuse(err, YVEX_ERR_FORMAT, "mutated or unsupported source geometry");
    b.rc = yvex_ir_module_open(&b.module, "lfm2_moe", source, dialects, 3u, err);
    yvex_ir_dimension rows = {.name = "sequence", .minimum = 1u, .maximum = a->context, .multiple = 1u};
    if (b.rc == YVEX_OK) b.rc = yvex_ir_dimension_add(b.module, &rows, &b.sequence, err);
    b.hidden = lfm_rows(&b, a->hidden, YVEX_IR_BF16);
    b.position = lfm_type(&b, YVEX_IR_SCALAR, YVEX_IR_INDEX, NULL, 0u, NULL, 0);
    uint64_t conv[] = {a->hidden, a->convolution_kernel}, kv[] = {2u, a->context, a->kv_heads, a->head_width};
    b.convolution = lfm_type(&b, YVEX_IR_STATE, YVEX_IR_F32, conv, 2u, "convolution.causal", 0);
    b.kv = lfm_type(&b, YVEX_IR_STATE, YVEX_IR_BF16, kv, 4u, "attention.causal_kv", 0);
    if (b.rc == YVEX_OK) b.rc = lfm_functions(&b);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_seal(b.module, err);
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    if (b.rc == YVEX_OK) b.rc = yvex_ir_pass_pipeline(b.module, passes, 2u, out, NULL, err);
    yvex_ir_module_close(&b.module);
    return b.rc;
}

const yvex_lfm2_api *yvex_model_register_lfm2(void)
{
    static const yvex_lfm2_api api = {2u, lfm_config_parse, lfm_open, lfm_classify,
        lfm_table_audit, lfm_snapshot_audit, lfm_program_build};
    return &api;
}
