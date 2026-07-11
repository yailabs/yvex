/*
 * yvex_source_provenance.c - exact source and manifest provenance owner.
 *
 * Owner: src/source.
 * Owns: manifest parsing, local Hugging Face metadata, revision consistency,
 *   and Git-blob identity verification for the pinned upstream index.
 * Does not own: source downloads, config parsing, header inventory, writes, or rendering.
 * Invariants: exact revisions and index OIDs fail closed; payload shards are never hashed here.
 * Boundary: provider metadata verification is not full payload trust.
 */
#define _XOPEN_SOURCE 700
#include "yvex_source_provenance.h"

#include "yvex_source_json.h"
#include "yvex_source_verify_internal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_MANIFEST_CAP (32u * 1024u * 1024u)

typedef struct {
    uint32_t state[5];
    unsigned long long length;
    unsigned char block[64];
    size_t block_length;
} source_sha1;

static uint32_t source_sha1_rotl(uint32_t value, unsigned int bits)
{
    return (value << bits) | (value >> (32u - bits));
}

/* Applies one SHA-1 compression block to caller-owned Git identity state. */
static void source_sha1_transform(source_sha1 *ctx,
                                  const unsigned char block[64])
{
    uint32_t words[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    unsigned int i;

    for (i = 0u; i < 16u; ++i) {
        words[i] = ((uint32_t)block[i * 4u] << 24u) |
                   ((uint32_t)block[i * 4u + 1u] << 16u) |
                   ((uint32_t)block[i * 4u + 2u] << 8u) |
                   (uint32_t)block[i * 4u + 3u];
    }
    for (i = 16u; i < 80u; ++i) {
        words[i] = source_sha1_rotl(words[i - 3u] ^ words[i - 8u] ^
                                    words[i - 14u] ^ words[i - 16u], 1u);
    }
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    for (i = 0u; i < 80u; ++i) {
        uint32_t f;
        uint32_t k;
        uint32_t temp;

        if (i < 20u) {
            f = (b & c) | ((~b) & d);
            k = 0x5a827999u;
        } else if (i < 40u) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        } else if (i < 60u) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }
        temp = source_sha1_rotl(a, 5u) + f + e + k + words[i];
        e = d;
        d = c;
        c = source_sha1_rotl(b, 30u);
        b = a;
        a = temp;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

static void source_sha1_init(source_sha1 *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xc3d2e1f0u;
}

/* Adds bounded bytes to SHA-1 state without owning the input buffer. */
static void source_sha1_update(source_sha1 *ctx,
                               const unsigned char *data,
                               size_t length)
{
    size_t offset = 0u;

    ctx->length += (unsigned long long)length;
    while (offset < length) {
        size_t available = sizeof(ctx->block) - ctx->block_length;
        size_t take = length - offset < available ? length - offset : available;
        memcpy(ctx->block + ctx->block_length, data + offset, take);
        ctx->block_length += take;
        offset += take;
        if (ctx->block_length == sizeof(ctx->block)) {
            source_sha1_transform(ctx, ctx->block);
            ctx->block_length = 0u;
        }
    }
}

/* Finalizes SHA-1 padding and writes the fixed-size digest to the caller. */
static void source_sha1_final(source_sha1 *ctx, unsigned char digest[20])
{
    unsigned long long bits = ctx->length * 8ull;
    unsigned int i;

    ctx->block[ctx->block_length++] = 0x80u;
    if (ctx->block_length > 56u) {
        memset(ctx->block + ctx->block_length, 0,
               sizeof(ctx->block) - ctx->block_length);
        source_sha1_transform(ctx, ctx->block);
        ctx->block_length = 0u;
    }
    memset(ctx->block + ctx->block_length, 0, 56u - ctx->block_length);
    for (i = 0u; i < 8u; ++i) {
        ctx->block[63u - i] = (unsigned char)((bits >> (i * 8u)) & 0xffu);
    }
    source_sha1_transform(ctx, ctx->block);
    for (i = 0u; i < 5u; ++i) {
        digest[i * 4u] = (unsigned char)(ctx->state[i] >> 24u);
        digest[i * 4u + 1u] = (unsigned char)(ctx->state[i] >> 16u);
        digest[i * 4u + 2u] = (unsigned char)(ctx->state[i] >> 8u);
        digest[i * 4u + 3u] = (unsigned char)ctx->state[i];
    }
}

/* Computes Git's SHA-1 blob identity for one bounded metadata file. */
int yvex_source_git_blob_oid_file(const char *path,
                                  char out_hex[41],
                                  yvex_error *err)
{
    unsigned long long size;
    char header[64];
    int header_length;
    FILE *fp;
    unsigned char buffer[16384];
    unsigned char digest[20];
    source_sha1 ctx;
    size_t got;
    unsigned int i;
    static const char hex[] = "0123456789abcdef";

    if (!path || !out_hex || !yvex_source_regular_file(path, &size)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_git_blob_oid",
                       "a regular metadata file and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    header_length = snprintf(header, sizeof(header), "blob %llu", size);
    if (header_length < 0 || (size_t)header_length + 1u > sizeof(header)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "source_git_blob_oid",
                       "Git blob header overflow");
        return YVEX_ERR_BOUNDS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_git_blob_oid",
                        "cannot open metadata file: %s", path);
        return YVEX_ERR_IO;
    }
    source_sha1_init(&ctx);
    source_sha1_update(&ctx, (const unsigned char *)header,
                       (size_t)header_length + 1u);
    while ((got = fread(buffer, 1u, sizeof(buffer), fp)) > 0u) {
        source_sha1_update(&ctx, buffer, got);
    }
    if (ferror(fp) || fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_git_blob_oid",
                        "cannot read metadata file: %s", path);
        return YVEX_ERR_IO;
    }
    source_sha1_final(&ctx, digest);
    for (i = 0u; i < 20u; ++i) {
        out_hex[i * 2u] = hex[digest[i] >> 4u];
        out_hex[i * 2u + 1u] = hex[digest[i] & 0x0fu];
    }
    out_hex[40] = '\0';
    return YVEX_OK;
}

/* Parses exact repository and revision declarations from a manifest source. */
static int source_manifest_parse_source(yvex_source_json *json,
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
            return (seen & 2u) == 2u;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "kind") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->source_kind, sizeof(out->source_kind))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "repo") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    json, out->repository_id,
                    sizeof(out->repository_id))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "revision") == 0) {
            if ((seen & 4u) || !yvex_source_json_string(
                    json, out->manifest_revision,
                    sizeof(out->manifest_revision))) return 0;
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

/* Parses the manifest local path without resolving or trusting it. */
static int source_manifest_parse_local(yvex_source_json *json,
                                       char *path,
                                       size_t cap)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    int seen = 0;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "path") == 0) {
            if (seen || !yvex_source_json_string(json, path, cap)) return 0;
            seen = 1;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses the canonical target identity declared by a verifier-owned manifest. */
static int source_manifest_parse_target(yvex_source_json *json,
                                        yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    int seen = 0;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "id") == 0) {
            if (seen || !yvex_source_json_string(
                    json, out->manifest_target_id,
                    sizeof(out->manifest_target_id))) return 0;
            seen = 1;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses verifier stage, inventory authority, counts, and payload non-trust. */
static int source_manifest_parse_verification(yvex_source_json *json,
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
            return seen == 2047u;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "stage") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->verification_stage,
                    sizeof(out->verification_stage))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "inventory_authority") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    json, out->inventory_authority,
                    sizeof(out->inventory_authority))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "source_file_count") == 0) {
            if ((seen & 4u) || !yvex_source_json_u64(
                    json, &out->manifest_source_file_count)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "source_total_bytes") == 0) {
            if ((seen & 8u) || !yvex_source_json_u64(
                    json, &out->manifest_source_total_bytes)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "shard_count") == 0) {
            if ((seen & 16u) || !yvex_source_json_u64(
                    json, &out->manifest_shard_count)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "shard_bytes") == 0) {
            if ((seen & 32u) || !yvex_source_json_u64(
                    json, &out->manifest_shard_bytes)) return 0;
            seen |= 32u;
        } else if (strcmp(key, "header_tensor_count") == 0) {
            if ((seen & 64u) || !yvex_source_json_u64(
                    json, &out->manifest_header_tensor_count)) return 0;
            seen |= 64u;
        } else if (strcmp(key, "config_status") == 0) {
            if ((seen & 128u) || !yvex_source_json_string(
                    json, out->manifest_config_status,
                    sizeof(out->manifest_config_status))) return 0;
            seen |= 128u;
        } else if (strcmp(key, "tokenizer_status") == 0) {
            if ((seen & 256u) || !yvex_source_json_string(
                    json, out->manifest_tokenizer_status,
                    sizeof(out->manifest_tokenizer_status))) return 0;
            seen |= 256u;
        } else if (strcmp(key, "payload_digest_status") == 0) {
            if ((seen & 512u) || !yvex_source_json_string(
                    json, out->manifest_payload_digest_status,
                    sizeof(out->manifest_payload_digest_status))) return 0;
            seen |= 512u;
        } else if (strcmp(key, "upstream_index_oid") == 0) {
            if ((seen & 1024u) || !yvex_source_json_string(
                    json, out->upstream_index_oid,
                    sizeof(out->upstream_index_oid))) return 0;
            seen |= 1024u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses one v1 or v2 manifest and rejects duplicate or incomplete known facts. */
static int source_manifest_parse(const char *data,
                                 size_t length,
                                 yvex_source_verification *out,
                                 char *local_path,
                                 size_t local_path_cap)
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
        if (strcmp(key, "schema") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    &json, out->manifest_schema,
                    sizeof(out->manifest_schema))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "status") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    &json, out->manifest_status,
                    sizeof(out->manifest_status))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "source") == 0) {
            if ((seen & 4u) || !source_manifest_parse_source(&json, out)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "local") == 0) {
            if ((seen & 8u) || !source_manifest_parse_local(
                    &json, local_path, local_path_cap)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "target") == 0) {
            if ((seen & 16u) || !source_manifest_parse_target(&json, out)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "verification") == 0) {
            if ((seen & 32u) || !source_manifest_parse_verification(
                    &json, out)) return 0;
            seen |= 32u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    if (!yvex_source_json_complete(&json) || (seen & 15u) != 15u) return 0;
    if (strcmp(out->manifest_schema, "yvex.source_manifest.v1") == 0) {
        return (seen & 48u) == 0u;
    }
    return strcmp(out->manifest_schema, "yvex.source_manifest.v2") == 0 &&
           (seen & 48u) == 48u;
}

/* Resolves explicit or canonical external manifest placement without source mutation. */
static int source_manifest_path(const yvex_source_verify_options *options,
                                char *out,
                                size_t cap)
{
    char candidate[YVEX_PATH_CAP];
    int n;

    if (options->manifest_path && options->manifest_path[0]) {
        n = snprintf(out, cap, "%s", options->manifest_path);
        return n >= 0 && (size_t)n < cap;
    }
    if (options->promote_manifest && options->models_root) {
        n = snprintf(out, cap, "%s/gguf/%s/deepseek-source-manifest.json",
                     options->models_root, options->identity->family_key);
        return n >= 0 && (size_t)n < cap;
    }
    if (yvex_source_path_join(candidate, sizeof(candidate),
                              options->source_path, "source-manifest.json") &&
        yvex_source_regular_file(candidate, NULL)) {
        n = snprintf(out, cap, "%s", candidate);
        return n >= 0 && (size_t)n < cap;
    }
    if (yvex_source_path_join(candidate, sizeof(candidate),
                              options->source_path, "source_manifest.json") &&
        yvex_source_regular_file(candidate, NULL)) {
        n = snprintf(out, cap, "%s", candidate);
        return n >= 0 && (size_t)n < cap;
    }
    n = options->models_root
            ? snprintf(out, cap, "%s/gguf/%s/deepseek-source-manifest.json",
                       options->models_root, options->identity->family_key)
            : -1;
    return n >= 0 && (size_t)n < cap;
}

/* Reads and parses the current manifest while preserving missing/stale blockers. */
int yvex_source_provenance_manifest_read(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_error *err)
{
    char *data;
    size_t length;
    char manifest_local[YVEX_PATH_CAP] = "";
    char resolved_manifest_local[YVEX_PATH_CAP];

    if (!options || !out || !source_manifest_path(
            options, out->manifest_path, sizeof(out->manifest_path)) ||
        !yvex_source_regular_file(out->manifest_path, NULL)) {
        yvex_source_verification_add_blocker(out, "missing-source-manifest");
        return YVEX_OK;
    }
    data = yvex_source_read_bounded_file(out->manifest_path,
                                         SOURCE_MANIFEST_CAP, &length, err);
    if (!data) {
        if (yvex_error_code(err) == YVEX_ERR_NOMEM) return YVEX_ERR_NOMEM;
        yvex_source_verification_add_blocker(out, "malformed-source-manifest");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!source_manifest_parse(data, length, out, manifest_local,
                               sizeof(manifest_local))) {
        free(data);
        yvex_source_verification_add_blocker(out, "malformed-source-manifest");
        return YVEX_OK;
    }
    free(data);
    if (strcmp(out->source_kind, "huggingface") != 0) {
        yvex_source_verification_add_blocker(
            out, out->source_kind[0] ? "unsupported-source-kind"
                                     : "missing-source-kind");
    }
    if (strcmp(out->repository_id, options->identity->upstream_repo_id) != 0) {
        yvex_source_verification_add_blocker(out, "wrong-source-repository");
    } else {
        out->repository_verified = 1;
    }
    if (!realpath(manifest_local, resolved_manifest_local) ||
        strcmp(resolved_manifest_local, out->resolved_source_path) != 0) {
        yvex_source_verification_add_blocker(out, "wrong-source-local-path");
    }
    if (out->manifest_target_id[0] &&
        strcmp(out->manifest_target_id, options->identity->target_id) != 0) {
        yvex_source_verification_add_blocker(out, "wrong-source-target");
    }
    return YVEX_OK;
}

/* Reads Hugging Face provider metadata for one local snapshot file. */
static int source_metadata_read(const char *source_path,
                                const char *name,
                                char *revision,
                                size_t revision_cap,
                                char *oid,
                                size_t oid_cap)
{
    char cache[YVEX_PATH_CAP];
    char path[YVEX_PATH_CAP];
    FILE *fp;
    char line[160];
    int n;

    if (!yvex_source_path_join(cache, sizeof(cache), source_path,
                               ".cache/huggingface/download")) return 0;
    n = snprintf(path, sizeof(path), "%s/%s.metadata", cache, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    fp = fopen(path, "rb");
    if (!fp || !fgets(line, sizeof(line), fp)) {
        if (fp) fclose(fp);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (!yvex_source_revision_is_commit(line) ||
        snprintf(revision, revision_cap, "%s", line) < 0) {
        fclose(fp);
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (!line[0] || snprintf(oid, oid_cap, "%s", line) < 0) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int source_oid_is_sha1(const char *oid)
{
    size_t i;

    if (!oid || strlen(oid) != 40u) return 0;
    for (i = 0u; i < 40u; ++i) {
        if (!isxdigit((unsigned char)oid[i])) return 0;
    }
    return 1;
}

/* Verifies one sidecar revision and, when requested, its pinned Git blob OID. */
int yvex_source_provenance_verify_file(
    const yvex_source_verify_options *options,
    const char *name,
    int verify_upstream_index,
    yvex_source_verification *out,
    yvex_error *err)
{
    char revision[128];
    char oid[128];
    char path[YVEX_PATH_CAP];
    unsigned long long size;
    int rc;

    if (!options || !name || !out || !source_metadata_read(
            options->source_path, name, revision, sizeof(revision),
            oid, sizeof(oid))) {
        yvex_source_verification_add_blocker(out, "missing-source-revision");
        return YVEX_OK;
    }
    if (!out->revision[0]) {
        snprintf(out->revision, sizeof(out->revision), "%s", revision);
    } else if (strcmp(out->revision, revision) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "inconsistent-source-revision");
    }
    if (strcmp(revision, options->identity->upstream_revision) != 0) {
        yvex_source_verification_add_blocker(out, "stale-source-revision");
    }
    if (!verify_upstream_index) return YVEX_OK;
    if (!source_oid_is_sha1(oid)) {
        yvex_source_verification_add_blocker(out,
                                             "upstream-index-identity-mismatch");
        return YVEX_OK;
    }
    memcpy(out->upstream_index_oid, oid, 41u);
    if (!yvex_source_path_join(path, sizeof(path), options->source_path, name) ||
        !yvex_source_regular_file(path, &size) ||
        size != options->identity->upstream_index_size ||
        strcmp(oid, options->identity->upstream_index_oid) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "upstream-index-identity-mismatch");
        return YVEX_OK;
    }
    rc = yvex_source_git_blob_oid_file(path, out->local_index_oid, err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(out->local_index_oid, oid) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "upstream-index-identity-mismatch");
    } else {
        out->upstream_index_identity_verified = 1;
    }
    return YVEX_OK;
}

/* Reconciles all observed provider revisions into exact repository provenance. */
void yvex_source_provenance_finalize(
    const yvex_source_verify_options *options,
    yvex_source_verification *out)
{
    size_t i;
    int manifest_ref_valid = 1;

    if (!options || !out) return;
    if (!out->manifest_revision[0]) {
        yvex_source_verification_add_blocker(out, "missing-source-revision");
        manifest_ref_valid = 0;
    } else if (strcmp(out->manifest_revision, "unknown") == 0 ||
               strcmp(out->manifest_revision, "unverified") == 0) {
        yvex_source_verification_add_blocker(out,
                                             "unverifiable-source-revision");
        manifest_ref_valid = 0;
    } else {
        for (i = 0u; out->manifest_revision[i]; ++i) {
            unsigned char ch = (unsigned char)out->manifest_revision[i];
            if (!isalnum(ch) && ch != '.' && ch != '_' && ch != '-' &&
                ch != '/') {
                yvex_source_verification_add_blocker(
                    out, "unverifiable-source-revision");
                manifest_ref_valid = 0;
                break;
            }
        }
    }
    if (!out->revision[0] ||
        strcmp(out->revision, options->identity->upstream_revision) != 0 ||
        yvex_source_verification_has_blocker(out, "missing-source-revision") ||
        yvex_source_verification_has_blocker(out,
                                             "inconsistent-source-revision") ||
        yvex_source_verification_has_blocker(out, "stale-source-revision") ||
        !manifest_ref_valid) {
        out->revision_verified = 0;
        return;
    }
    if (out->manifest_revision[0] &&
        yvex_source_revision_is_commit(out->manifest_revision) &&
        strcmp(out->manifest_revision, out->revision) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "inconsistent-source-revision");
        return;
    }
    out->revision_verified = 1;
}

/* Checks every verifier-owned manifest field against current canonical facts. */
int yvex_source_provenance_manifest_matches(
    const yvex_source_verify_options *options,
    const yvex_source_verification *out)
{
    return options && out &&
           strcmp(out->manifest_schema, "yvex.source_manifest.v2") == 0 &&
           strcmp(out->manifest_status, "complete") == 0 &&
           strcmp(out->manifest_target_id, options->identity->target_id) == 0 &&
           strcmp(out->repository_id, options->identity->upstream_repo_id) == 0 &&
           strcmp(out->manifest_revision, out->revision) == 0 &&
           strcmp(out->verification_stage,
                  "exact-source-metadata-header-verified") == 0 &&
           strcmp(out->inventory_authority,
                  options->identity->upstream_inventory_authority) == 0 &&
           strcmp(out->manifest_config_status, "verified") == 0 &&
           strcmp(out->manifest_tokenizer_status, "verified") == 0 &&
           strcmp(out->manifest_payload_digest_status, "not-verified") == 0 &&
           strcmp(out->upstream_index_oid,
                  options->identity->upstream_index_oid
                      ? options->identity->upstream_index_oid
                      : "not-applicable") == 0 &&
           out->manifest_source_file_count == out->source_file_count &&
           out->manifest_source_total_bytes == out->source_total_bytes &&
           out->manifest_shard_count == out->shard_count &&
           out->manifest_shard_bytes == out->shard_bytes &&
           out->manifest_header_tensor_count == out->header_tensor_count;
}
