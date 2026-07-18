/*
 * verify.c - exact source verification and sidecar admission owner.
 *
 * Owner: src/source.
 * Owns: final verification policy, bounded family sidecar fact extraction,
 *   blocker facts, phase coordination, and manifest promotion.
 * Does not own: JSON primitives, provenance parsing, shard/header inventory,
 *   file serialization, rendering, architecture policy, or payload reads.
 * Invariants: each specialized owner is consumed once; strict success requires
 *   a reopened complete manifest and one canonical header scan.
 * Boundary: exact source verification is not artifact, runtime, or generation support.
 */
#define _XOPEN_SOURCE 700
#include "verify.h"

#include "inventory.h"
#include "json.h"
#include "private.h"
#include "provenance.h"
#include "write.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SOURCE_CONFIG_CAP (1024u * 1024u)
#define SOURCE_TOKENIZER_CAP (32u * 1024u * 1024u)

typedef enum {
    SOURCE_SIDECAR_CONFIG = 0,
    SOURCE_SIDECAR_TOKENIZER,
    SOURCE_SIDECAR_TOKENIZER_CONFIG,
    SOURCE_SIDECAR_GENERATION_CONFIG
} source_sidecar_kind;

static int source_parse_sidecar(
    source_sidecar_kind kind,
    const char *data,
    size_t length,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out);

int yvex_source_path_join(char *out,
                          size_t cap,
                          const char *left,
                          const char *right)
{
    int n;

    if (!out || cap == 0u || !left || !right) return 0;
    n = snprintf(out, cap, "%s%s%s", left,
                 left[0] && left[strlen(left) - 1u] == '/' ? "" : "/",
                 right);
    return n >= 0 && (size_t)n < cap;
}

int yvex_source_regular_file(const char *path, unsigned long long *size)
{
    struct stat st;

    if (!path || lstat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size < 0) return 0;
    if (size) *size = (unsigned long long)st.st_size;
    return 1;
}

int yvex_source_revision_is_commit(const char *text)
{
    size_t i;

    if (!text || strlen(text) != 40u) return 0;
    for (i = 0u; i < 40u; ++i) {
        if (!isxdigit((unsigned char)text[i])) return 0;
    }
    return 1;
}

int yvex_source_checked_add_u64(unsigned long long *total,
                                unsigned long long value)
{
    if (!total || ULLONG_MAX - *total < value) return 0;
    *total += value;
    return 1;
}

void yvex_source_verification_add_blocker(yvex_source_verification *out,
                                          const char *reason)
{
    unsigned int i;

    if (!out || !reason) return;
    for (i = 0u; i < out->blocker_count; ++i) {
        if (strcmp(out->blockers[i], reason) == 0) return;
    }
    if (out->blocker_count < YVEX_SOURCE_VERIFY_BLOCKER_CAP) {
        out->blockers[out->blocker_count++] = reason;
    }
}

int yvex_source_verification_has_blocker(
    const yvex_source_verification *out,
    const char *reason)
{
    unsigned int i;

    if (!out || !reason) return 0;
    for (i = 0u; i < out->blocker_count; ++i) {
        if (strcmp(out->blockers[i], reason) == 0) return 1;
    }
    return 0;
}

void yvex_source_verification_remove_blocker(yvex_source_verification *out,
                                             const char *reason)
{
    unsigned int i;

    if (!out || !reason) return;
    for (i = 0u; i < out->blocker_count; ++i) {
        if (strcmp(out->blockers[i], reason) == 0) {
            unsigned int j;
            for (j = i + 1u; j < out->blocker_count; ++j) {
                out->blockers[j - 1u] = out->blockers[j];
            }
            out->blocker_count--;
            return;
        }
    }
}

/* Reuses the source scanner for recursive checked metadata footprint facts. */
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
        yvex_source_verification_add_blocker(out,
                                             "source-footprint-overflow");
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

static const char *source_sidecar_name(
    source_sidecar_kind kind)
{
    switch (kind) {
    case SOURCE_SIDECAR_CONFIG: return "config.json";
    case SOURCE_SIDECAR_TOKENIZER: return "tokenizer.json";
    case SOURCE_SIDECAR_TOKENIZER_CONFIG: return "tokenizer_config.json";
    case SOURCE_SIDECAR_GENERATION_CONFIG: return "generation_config.json";
    default: return "";
    }
}

static const char *source_sidecar_missing(
    source_sidecar_kind kind)
{
    switch (kind) {
    case SOURCE_SIDECAR_CONFIG: return "missing-source-config";
    case SOURCE_SIDECAR_TOKENIZER: return "missing-tokenizer-json";
    case SOURCE_SIDECAR_TOKENIZER_CONFIG: return "missing-tokenizer-config";
    case SOURCE_SIDECAR_GENERATION_CONFIG: return "missing-generation-config";
    default: return "missing-source-sidecar";
    }
}

static const char *source_sidecar_malformed(
    source_sidecar_kind kind)
{
    switch (kind) {
    case SOURCE_SIDECAR_CONFIG: return "malformed-source-config";
    case SOURCE_SIDECAR_TOKENIZER: return "malformed-tokenizer-json";
    case SOURCE_SIDECAR_TOKENIZER_CONFIG: return "malformed-tokenizer-config";
    case SOURCE_SIDECAR_GENERATION_CONFIG: return "malformed-generation-config";
    default: return "malformed-source-sidecar";
    }
}

/* Reads, structurally parses, and provenance-checks one required sidecar. */
static int source_verify_sidecar(
    const yvex_source_verify_options *options,
    source_sidecar_kind kind,
    yvex_source_verification *out,
    yvex_error *err)
{
    const char *name = source_sidecar_name(kind);
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    size_t cap = kind == SOURCE_SIDECAR_TOKENIZER
                     ? SOURCE_TOKENIZER_CAP : SOURCE_CONFIG_CAP;
    int rc;

    if (!yvex_source_path_join(path, sizeof(path), options->source_path, name) ||
        !yvex_source_regular_file(path, NULL)) {
        yvex_source_verification_add_blocker(out,
                                             source_sidecar_missing(kind));
        return YVEX_OK;
    }
    data = yvex_source_read_bounded_file(path, cap, &length, err);
    if (!data) {
        if (yvex_error_code(err) == YVEX_ERR_NOMEM) return YVEX_ERR_NOMEM;
        yvex_source_verification_add_blocker(out,
                                             source_sidecar_malformed(kind));
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!source_parse_sidecar(
            kind, data, length, options->identity, out)) {
        yvex_source_verification_add_blocker(out,
                                             source_sidecar_malformed(kind));
    }
    free(data);
    rc = yvex_source_provenance_verify_file(options, name, 0, out, err);
    return rc;
}

static int source_manifest_is_current(
    const yvex_source_verify_options *options,
    const yvex_source_verification *out)
{
    return strcmp(out->manifest_status, "complete") == 0 &&
           yvex_source_provenance_manifest_matches(options, out);
}

/* Publishes and reopens the exact verified manifest through its writer owner. */
static int source_promote_manifest(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_error *err)
{
    int rc;

    snprintf(out->verification_stage, sizeof(out->verification_stage),
             "%s", "exact-source-metadata-header-verified");
    rc = yvex_source_manifest_publish_verified(out->manifest_path, options,
                                               out, err);
    if (rc != YVEX_OK) {
        yvex_source_verification_add_blocker(
            out, "source-manifest-publish-failed");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    out->manifest_published = 1;
    rc = yvex_source_provenance_manifest_read(options, out, err);
    if (rc != YVEX_OK) return rc;
    out->manifest_reopened = 1;
    if (!source_manifest_is_current(options, out)) {
        yvex_source_verification_add_blocker(
            out, "source-manifest-reopen-mismatch");
    } else {
        out->manifest_verified = 1;
    }
    return YVEX_OK;
}

const char *yvex_source_verification_status(
    const yvex_source_verification *verification)
{
    if (!verification) return "invalid";
    return verification->verified ? "verified" : "blocked";
}

/* Coordinates exact source owners, promotes verified facts, and returns typed blockers. */
int yvex_source_verify_with_snapshot(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_source_tensor_snapshot **snapshot,
    yvex_error *err)
{
    struct stat st;
    yvex_source_derived_inventory derived;
    yvex_source_tensor_snapshot *candidate_snapshot = NULL;
    int rc;
    unsigned int kind;

    if (!options || !out || !options->identity || !options->source_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_verify",
                       "identity, source path, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (snapshot) *snapshot = NULL;
    memset(&derived, 0, sizeof(derived));
    yvex_error_clear(err);

    if (lstat(options->source_path, &st) != 0) {
        yvex_source_verification_add_blocker(out, "missing-source-path");
        return YVEX_OK;
    }
    if (!S_ISDIR(st.st_mode) || !realpath(options->source_path,
                                         out->resolved_source_path)) {
        yvex_source_verification_add_blocker(out, "wrong-source-path-type");
        return YVEX_OK;
    }
    out->path_verified = 1;
    rc = yvex_source_provenance_manifest_read(options, out, err);
    if (rc != YVEX_OK) goto cleanup;
    for (kind = SOURCE_SIDECAR_CONFIG;
         kind <= SOURCE_SIDECAR_GENERATION_CONFIG; ++kind) {
        rc = source_verify_sidecar(
            options, (source_sidecar_kind)kind, out, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    if (out->config_valid && out->generation_config_valid &&
        (out->bos_token_id != out->generation_bos_token_id ||
         out->eos_token_id != out->generation_eos_token_id)) {
        yvex_source_verification_add_blocker(
            out, "generation-config-token-mismatch");
    }
    rc = source_verify_footprint(options->source_path, out, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_source_inventory_verify(options, out, &derived,
                                      &candidate_snapshot, err);
    if (rc != YVEX_OK) goto cleanup;
    if (candidate_snapshot) {
        yvex_source_tensor_snapshot_facts snapshot_facts;
        rc = yvex_source_tensor_snapshot_facts_get(
            candidate_snapshot, &snapshot_facts, err);
        if (rc != YVEX_OK) goto cleanup;
        out->source_snapshot_identity = snapshot_facts.identity;
    }
    yvex_source_provenance_finalize(options, out);

    if (strcmp(out->inventory_authority, "header-derived") == 0 &&
        out->blocker_count == 0u) {
        if (!options->derived_inventory_path) {
            yvex_source_verification_add_blocker(
                out, "missing-derived-inventory-output");
        } else {
            rc = yvex_source_derived_inventory_publish(
                options->derived_inventory_path, options, &derived, err);
            if (rc != YVEX_OK) {
                yvex_source_verification_add_blocker(
                    out, "derived-inventory-publish-failed");
                yvex_error_clear(err);
            }
        }
    }
    if (out->blocker_count == 0u && source_manifest_is_current(options, out)) {
        out->manifest_verified = 1;
        out->manifest_reopened = 1;
        out->manifest_payload_trusted =
            strcmp(out->manifest_schema, "yvex.source_manifest.v3") == 0;
    } else if (out->blocker_count == 0u &&
               strcmp(out->manifest_schema, "yvex.source_manifest.v3") == 0 &&
               out->manifest_payload_identity[0]) {
        yvex_source_verification_add_blocker(out,
                                             "payload-snapshot-drift");
    } else if (out->blocker_count == 0u && options->promote_manifest) {
        rc = source_promote_manifest(options, out, err);
        if (rc != YVEX_OK) goto cleanup;
    } else if (out->blocker_count == 0u) {
        yvex_source_verification_add_blocker(
            out, strcmp(out->manifest_status, "complete") == 0
                     ? "source-manifest-stale"
                     : "source-manifest-incomplete");
    }
    out->verified = out->blocker_count == 0u && out->path_verified &&
                    out->repository_verified && out->revision_verified &&
                    out->config_valid && out->tokenizer_json_valid &&
                    out->tokenizer_config_valid &&
                    out->generation_config_valid &&
                    out->shard_index_headers_match &&
                    out->header_scan_count == 1u && out->manifest_verified &&
                    (strcmp(out->inventory_authority, "header-derived") == 0 ||
                     out->upstream_index_identity_verified);
    if (out->verified && snapshot) {
        *snapshot = candidate_snapshot;
        candidate_snapshot = NULL;
    }
    rc = YVEX_OK;
cleanup:
    yvex_source_tensor_snapshot_release(candidate_snapshot);
    yvex_source_derived_inventory_free(&derived);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_source_verify(const yvex_source_verify_options *options,
                       yvex_source_verification *out,
                       yvex_error *err)
{
    yvex_source_tensor_snapshot *snapshot = NULL;
    int rc = yvex_source_verify_with_snapshot(options, out, &snapshot, err);

    yvex_source_tensor_snapshot_release(snapshot);
    return rc;
}
/* DeepSeek sidecar parsing publishes exact facts into source admission. */

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
            return seen == 31u;
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
        } else if (strcmp(key, "beta_fast") == 0) {
            if ((seen & 8u) || !yvex_source_json_u64(
                    json, &out->rope_beta_fast)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "beta_slow") == 0) {
            if ((seen & 16u) || !yvex_source_json_u64(
                    json, &out->rope_beta_slow)) return 0;
            seen |= 16u;
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
            return seen == 31u;
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
        } else if (strcmp(key, "activation_scheme") == 0) {
            if ((seen & 8u) || !yvex_source_json_string(
                    json, out->quant_activation_scheme,
                    sizeof(out->quant_activation_scheme))) return 0;
            seen |= 8u;
        } else if (strcmp(key, "scale_fmt") == 0) {
            if ((seen & 16u) || !yvex_source_json_string(
                    json, out->quant_scale_format,
                    sizeof(out->quant_scale_format))) return 0;
            seen |= 16u;
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

/* Records one tokenizer id while preserving checked effective-vocabulary facts. */
static int source_tokenizer_record_id(yvex_source_verification *out,
                                      unsigned long long token_id)
{
    if (!out || token_id == ULLONG_MAX) return 0;
    if (out->tokenizer_base_vocab_count == 0u &&
        out->tokenizer_added_token_count == 0u) {
        out->tokenizer_max_token_id = token_id;
    } else if (token_id > out->tokenizer_max_token_id) {
        out->tokenizer_max_token_id = token_id;
    }
    return 1;
}

/* Parses the tokenizer vocabulary object and its numeric id range. */
static int source_parse_tokenizer_vocab(yvex_source_json *json,
                                        yvex_source_verification *out)
{
    char token[YVEX_SOURCE_JSON_KEY_CAP];

    yvex_source_json_space(json);
    if (!json || !out || json->cursor >= json->end ||
        *json->cursor++ != '{') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == '}') {
        json->cursor++;
        return 1;
    }
    for (;;) {
        unsigned long long token_id;

        if (!yvex_source_json_string(json, token, sizeof(token))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':' ||
            !yvex_source_json_u64(json, &token_id) ||
            out->tokenizer_base_vocab_count == ULLONG_MAX ||
            !source_tokenizer_record_id(out, token_id)) return 0;
        out->tokenizer_base_vocab_count++;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') {
            json->cursor++;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses one added-token object and requires exactly one numeric id. */
static int source_parse_added_token(yvex_source_json *json,
                                    yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    int saw_id = 0;

    yvex_source_json_space(json);
    if (!json || !out || json->cursor >= json->end ||
        *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return saw_id;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "id") == 0) {
            unsigned long long token_id;
            if (saw_id || !yvex_source_json_u64(json, &token_id) ||
                !source_tokenizer_record_id(out, token_id)) return 0;
            saw_id = 1;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses the added-token array without materializing token strings. */
static int source_parse_added_tokens(yvex_source_json *json,
                                     yvex_source_verification *out)
{
    yvex_source_json_space(json);
    if (!json || !out || json->cursor >= json->end ||
        *json->cursor++ != '[') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') {
        json->cursor++;
        return 1;
    }
    for (;;) {
        if (!source_parse_added_token(json, out) ||
            out->tokenizer_added_token_count == ULLONG_MAX) return 0;
        out->tokenizer_added_token_count++;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
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
            if ((seen & 2u) ||
                !source_parse_tokenizer_vocab(json, out)) return 0;
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
            if ((seen & 2u) ||
                !source_parse_added_tokens(&json, out)) return 0;
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
    if (!yvex_source_json_complete(&json) || seen != 127u ||
        out->tokenizer_model_type[0] == '\0' ||
        (out->tokenizer_base_vocab_count == 0u &&
         out->tokenizer_added_token_count == 0u) ||
        out->tokenizer_max_token_id == ULLONG_MAX) return 0;
    out->tokenizer_effective_vocab_size = out->tokenizer_max_token_id + 1u;
    return 1;
}

static int source_parse_sidecar(
    source_sidecar_kind kind,
    const char *data,
    size_t length,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out)
{
    int valid;

    if (!data || !identity || !out) return 0;
    switch (kind) {
    case SOURCE_SIDECAR_CONFIG:
        return source_parse_config_json(data, length, identity, out);
    case SOURCE_SIDECAR_TOKENIZER:
        valid = source_parse_tokenizer_json(data, length, out);
        out->tokenizer_json_valid = valid;
        return valid;
    case SOURCE_SIDECAR_TOKENIZER_CONFIG:
        valid = source_parse_tokenizer_config(data, length, out);
        out->tokenizer_config_valid = valid;
        return valid;
    case SOURCE_SIDECAR_GENERATION_CONFIG:
        valid = source_parse_generation_config(data, length, out);
        out->generation_config_valid = valid;
        return valid;
    default:
        return 0;
    }
}
