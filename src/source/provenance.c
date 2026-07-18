/*
 * provenance.c - exact source and manifest provenance owner.
 *
 * Owner: src/source.
 * Owns: manifest parsing, local Hugging Face metadata, revision consistency,
 *   and Git-blob identity verification for the pinned upstream index.
 * Does not own: source downloads, config parsing, header inventory, writes, or rendering.
 * Invariants: exact revisions and index OIDs fail closed; payload shards are never hashed here.
 * Boundary: provider metadata verification is not full payload trust.
 */
#define _XOPEN_SOURCE 700
#include "provenance.h"

#include "json.h"
#include "private.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/core/sha256.h"

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
    int read_failed;
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
    read_failed = ferror(fp);
    if (fclose(fp) != 0) read_failed = 1;
    if (read_failed) {
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

/* Computes Git's blob identity over the exact bytes retained by a caller. */
static int source_git_blob_oid_bytes(const unsigned char *bytes,
                                     size_t byte_count,
                                     char out_hex[41])
{
    char header[64];
    int header_length;
    unsigned char digest[20];
    source_sha1 ctx;
    unsigned int index;
    static const char hex[] = "0123456789abcdef";

    if ((!bytes && byte_count) || !out_hex) return 0;
    header_length = snprintf(header, sizeof(header), "blob %llu",
                             (unsigned long long)byte_count);
    if (header_length < 0 || (size_t)header_length + 1u > sizeof(header))
        return 0;
    source_sha1_init(&ctx);
    source_sha1_update(&ctx, (const unsigned char *)header,
                       (size_t)header_length + 1u);
    source_sha1_update(&ctx, bytes, byte_count);
    source_sha1_final(&ctx, digest);
    for (index = 0u; index < sizeof(digest); ++index) {
        out_hex[index * 2u] = hex[digest[index] >> 4u];
        out_hex[index * 2u + 1u] = hex[digest[index] & 0x0fu];
    }
    out_hex[40] = '\0';
    return 1;
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

/* Accepts only one root-relative canonical shard basename without allocation. */
static int source_manifest_payload_name_valid(const char *name)
{
    const char *cursor;

    if (!name || !name[0] || name[0] == '/' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) return 0;
    for (cursor = name; *cursor; ++cursor) {
        if (*cursor == '/' || *cursor == '\\' || *cursor == '\n' ||
            *cursor == '\r') return 0;
    }
    return 1;
}

/* Validates one published payload shard and returns stable aggregate facts. */
static int source_manifest_parse_payload_shard(
    yvex_source_json *json,
    unsigned long long expected_id,
    char previous_name[YVEX_PATH_CAP],
    unsigned long long *file_bytes_out,
    int *upstream_trust_out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    char text[YVEX_PATH_CAP] = "";
    char expected[65] = "";
    char observed[65] = "";
    char algorithm[24] = "";
    char authority[40] = "";
    char trust[40] = "";
    unsigned long long id = ULLONG_MAX;
    unsigned long long file_bytes = 0u;
    unsigned long long data_region_offset = 0u;
    unsigned long long payload_bytes = 0u;
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            if (seen != 1023u || id != expected_id ||
                !source_manifest_payload_name_valid(text) ||
                (previous_name[0] && strcmp(previous_name, text) >= 0) ||
                file_bytes == 0u || data_region_offset > file_bytes ||
                payload_bytes != file_bytes - data_region_offset ||
                strcmp(algorithm, "sha256") != 0 || !authority[0] ||
                !yvex_sha256_hex_valid(observed) ||
                (expected[0] && (!yvex_sha256_hex_valid(expected) ||
                                 strcmp(expected, observed) != 0)) ||
                ((expected[0] &&
                  strcmp(trust, "upstream_payload_verified") != 0) ||
                 (!expected[0] &&
                  (strcmp(trust, "local_payload_snapshot_sealed") != 0 ||
                   strcmp(authority, "local-snapshot-seal") != 0))))
                return 0;
            if (snprintf(previous_name, YVEX_PATH_CAP, "%s", text) < 0)
                return 0;
            *file_bytes_out = file_bytes;
            *upstream_trust_out = expected[0] != '\0';
            return 1;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "id") == 0) {
            if ((seen & 1u) || !yvex_source_json_u64(json, &id)) return 0;
            seen |= 1u;
        } else if (strcmp(key, "name") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    json, text, sizeof(text))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "file_bytes") == 0) {
            if ((seen & 4u) || !yvex_source_json_u64(json, &file_bytes)) return 0;
            seen |= 4u;
        } else if (strcmp(key, "data_region_offset") == 0) {
            if ((seen & 8u) || !yvex_source_json_u64(
                    json, &data_region_offset)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "payload_bytes") == 0) {
            if ((seen & 16u) || !yvex_source_json_u64(
                    json, &payload_bytes)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "digest_algorithm") == 0) {
            if ((seen & 32u) || !yvex_source_json_string(
                    json, algorithm, sizeof(algorithm))) return 0;
            seen |= 32u;
        } else if (strcmp(key, "digest_authority") == 0) {
            if ((seen & 64u) || !yvex_source_json_string(
                    json, authority, sizeof(authority))) return 0;
            seen |= 64u;
        } else if (strcmp(key, "expected_digest") == 0) {
            if (seen & 128u) return 0;
            yvex_source_json_space(json);
            if (json->cursor < json->end && *json->cursor == '"') {
                if (!yvex_source_json_string(json, expected,
                                             sizeof(expected))) return 0;
            } else {
                static const char null_value[] = "null";
                if ((size_t)(json->end - json->cursor) <
                        sizeof(null_value) - 1u ||
                    memcmp(json->cursor, null_value,
                           sizeof(null_value) - 1u) != 0) return 0;
                json->cursor += sizeof(null_value) - 1u;
            }
            seen |= 128u;
        } else if (strcmp(key, "observed_digest") == 0) {
            if ((seen & 256u) || !yvex_source_json_string(
                    json, observed, sizeof(observed))) return 0;
            seen |= 256u;
        } else if (strcmp(key, "trust_class") == 0) {
            if ((seen & 512u) || !yvex_source_json_string(
                    json, trust, sizeof(trust))) return 0;
            seen |= 512u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses deterministic canonical-order shard rows and returns their exact count. */
static int source_manifest_parse_payload_shards(
    yvex_source_json *json,
    unsigned long long *count,
    unsigned long long *total_file_bytes)
{
    unsigned long long index = 0u;
    unsigned long long total = 0u;
    char previous_name[YVEX_PATH_CAP] = "";
    int all_upstream_trust = 1;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        unsigned long long file_bytes;
        int upstream_trust;

        if (!source_manifest_parse_payload_shard(
                json, index, previous_name, &file_bytes, &upstream_trust) ||
            ULLONG_MAX - total < file_bytes) return 0;
        if (!upstream_trust) all_upstream_trust = 0;
        total += file_bytes;
        index++;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            *count = index;
            *total_file_bytes = total;
            return all_upstream_trust ? 2 : 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses v3 aggregate payload identity and validates all published shard rows. */
static int source_manifest_parse_payload(yvex_source_json *json,
                                         yvex_source_verification *out)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned long long source_identity = 0u;
    unsigned long long tensor_count = 0u;
    unsigned long long logical_bytes = 0u;
    unsigned long long parsed_shards = 0u;
    unsigned long long parsed_file_bytes = 0u;
    int parsed_trust_class = 0;
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            out->manifest_payload_source_snapshot_identity = source_identity;
            out->manifest_payload_tensor_count = tensor_count;
            out->manifest_payload_logical_tensor_bytes = logical_bytes;
            return seen == 511u &&
                   yvex_sha256_hex_valid(out->manifest_payload_identity) &&
                   strcmp(out->manifest_payload_digest_algorithm,
                          "sha256") == 0 &&
                   (strcmp(out->manifest_payload_trust_class,
                           "upstream_payload_verified") == 0 ||
                    strcmp(out->manifest_payload_trust_class,
                           "local_payload_snapshot_sealed") == 0) &&
                   source_identity != 0u &&
                   out->manifest_payload_shard_count == parsed_shards &&
                   out->manifest_payload_bytes == parsed_file_bytes &&
                   ((parsed_trust_class == 2 &&
                     strcmp(out->manifest_payload_trust_class,
                            "upstream_payload_verified") == 0) ||
                    (parsed_trust_class == 1 &&
                     strcmp(out->manifest_payload_trust_class,
                            "local_payload_snapshot_sealed") == 0));
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "identity") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, out->manifest_payload_identity,
                    sizeof(out->manifest_payload_identity))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "trust_class") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    json, out->manifest_payload_trust_class,
                    sizeof(out->manifest_payload_trust_class))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "digest_algorithm") == 0) {
            if ((seen & 4u) || !yvex_source_json_string(
                    json, out->manifest_payload_digest_algorithm,
                    sizeof(out->manifest_payload_digest_algorithm))) return 0;
            seen |= 4u;
        } else if (strcmp(key, "source_snapshot_identity") == 0) {
            if ((seen & 8u) || !yvex_source_json_u64(
                    json, &source_identity)) return 0;
            seen |= 8u;
        } else if (strcmp(key, "shard_count") == 0) {
            if ((seen & 16u) || !yvex_source_json_u64(
                    json, &out->manifest_payload_shard_count)) return 0;
            seen |= 16u;
        } else if (strcmp(key, "shard_file_bytes") == 0) {
            if ((seen & 32u) || !yvex_source_json_u64(
                    json, &out->manifest_payload_bytes)) return 0;
            seen |= 32u;
        } else if (strcmp(key, "tensor_count") == 0) {
            if ((seen & 64u) || !yvex_source_json_u64(
                    json, &tensor_count)) return 0;
            seen |= 64u;
        } else if (strcmp(key, "logical_tensor_bytes") == 0) {
            if ((seen & 128u) || !yvex_source_json_u64(
                    json, &logical_bytes)) return 0;
            seen |= 128u;
        } else if (strcmp(key, "shards") == 0) {
            if (seen & 256u) return 0;
            parsed_trust_class = source_manifest_parse_payload_shards(
                json, &parsed_shards, &parsed_file_bytes);
            if (!parsed_trust_class) return 0;
            seen |= 256u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses one supported manifest and distinguishes unsupported schema versions. */
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
        } else if (strcmp(key, "payload") == 0) {
            if ((seen & 64u) || !source_manifest_parse_payload(
                    &json, out)) return 0;
            seen |= 64u;
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
        return seen == 15u;
    }
    if (strcmp(out->manifest_schema, "yvex.source_manifest.v2") == 0)
        return seen == 63u;
    if (strcmp(out->manifest_schema, "yvex.source_manifest.v3") == 0)
        return seen == 127u;
    return -1;
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
    {
        int parse_status = source_manifest_parse(
            data, length, out, manifest_local, sizeof(manifest_local));
        if (parse_status <= 0) {
            free(data);
            yvex_source_verification_add_blocker(
                out, parse_status < 0 ? "unsupported-source-manifest-version"
                                      : "malformed-source-manifest");
            return YVEX_OK;
        }
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

/* Projects an authoritative Hugging Face LFS SHA-256 bound to the pinned revision. */
int yvex_source_provenance_payload_digest(
    const yvex_source_verification *verification,
    const char *canonical_name,
    yvex_source_payload_digest_fact *out,
    yvex_error *err)
{
    char revision[128];
    char oid[128];
    size_t index;

    if (!verification || !canonical_name || !out ||
        !verification->resolved_source_path[0] || !verification->revision[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "source_payload_digest_provenance",
                       "verified source, canonical shard name, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!source_metadata_read(verification->resolved_source_path,
                              canonical_name, revision, sizeof(revision),
                              oid, sizeof(oid))) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    out->revision_matches = strcmp(revision, verification->revision) == 0;
    if (!out->revision_matches) {
        yvex_error_set(err, YVEX_ERR_FORMAT,
                       "source_payload_digest_provenance",
                       "payload provider metadata revision does not match verified source");
        return YVEX_ERR_FORMAT;
    }
    if (strlen(oid) != 64u) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!yvex_sha256_hex_valid(oid)) {
        yvex_error_set(err, YVEX_ERR_FORMAT,
                       "source_payload_digest_provenance",
                       "64-byte provider digest is not valid SHA-256 hexadecimal");
        return YVEX_ERR_FORMAT;
    }
    for (index = 0u; index < 64u; ++index)
        out->expected_digest[index] =
            (char)tolower((unsigned char)oid[index]);
    out->expected_digest[64] = '\0';
    snprintf(out->algorithm, sizeof(out->algorithm), "%s", "sha256");
    snprintf(out->authority, sizeof(out->authority), "%s",
             "huggingface-git-lfs-etag");
    out->available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
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

/* Accepts a single canonical source-root file name without path traversal. */
static int source_metadata_name_valid(const char *name)
{
    const char *cursor;

    if (!name || !name[0] || name[0] == '/' ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (cursor = name; *cursor; ++cursor)
        if (*cursor == '/' || *cursor == '\\' ||
            *cursor == '\n' || *cursor == '\r') return 0;
    return 1;
}

/*
 * Binds one small source sidecar to its pinned provider revision and Git blob
 * identity. It reads only the named metadata file, never tensor payload, and
 * publishes no partial identity fact on failure.
 */
int yvex_source_provenance_metadata_identity(
    const yvex_source_verification *verification,
    const char *canonical_name,
    yvex_source_metadata_identity_fact *out,
    yvex_error *err)
{
    yvex_source_metadata_identity_fact fact;
    char provider_revision[128];
    char provider_oid[128];
    char path[YVEX_PATH_CAP];
    unsigned long long file_bytes;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!verification || !out ||
        !verification->resolved_source_path[0] ||
        !verification->revision[0] ||
        !source_metadata_name_valid(canonical_name)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "source_metadata_identity",
                       "verified source and canonical metadata name are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&fact, 0, sizeof(fact));
    if (!source_metadata_read(verification->resolved_source_path,
                              canonical_name,
                              provider_revision, sizeof(provider_revision),
                              provider_oid, sizeof(provider_oid)) ||
        !source_oid_is_sha1(provider_oid)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "source_metadata_identity",
                       "provider metadata lacks a pinned Git blob identity");
        return YVEX_ERR_FORMAT;
    }
    fact.revision_matches =
        strcmp(provider_revision, verification->revision) == 0;
    if (!fact.revision_matches) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "source_metadata_identity",
                       "metadata provider revision differs from source snapshot");
        return YVEX_ERR_FORMAT;
    }
    if (!yvex_source_path_join(path, sizeof(path),
                               verification->resolved_source_path,
                               canonical_name) ||
        !yvex_source_regular_file(path, &file_bytes)) {
        yvex_error_set(err, YVEX_ERR_IO, "source_metadata_identity",
                       "metadata sidecar is missing or not a regular file");
        return YVEX_ERR_IO;
    }
    rc = yvex_source_git_blob_oid_file(
        path, fact.observed_git_blob_oid, err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(provider_oid, fact.observed_git_blob_oid) != 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "source_metadata_identity",
                       "metadata sidecar Git blob identity mismatch");
        return YVEX_ERR_FORMAT;
    }
    (void)snprintf(fact.canonical_name, sizeof(fact.canonical_name), "%s",
                   canonical_name);
    memcpy(fact.revision, provider_revision, 41u);
    memcpy(fact.expected_git_blob_oid, provider_oid, 41u);
    fact.file_bytes = file_bytes;
    fact.identity_verified = 1;
    *out = fact;
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Reads one already identity-verified metadata sidecar into bounded owned
 * memory. The caller releases the blob; short reads and post-read identity
 * changes fail closed without publishing bytes.
 */
int yvex_source_provenance_metadata_read(
    const yvex_source_verification *verification,
    const char *canonical_name,
    size_t maximum_bytes,
    yvex_source_metadata_blob *out,
    yvex_error *err)
{
    yvex_source_metadata_blob blob;
    yvex_source_metadata_identity_fact after;
    char path[YVEX_PATH_CAP];
    char retained_oid[41];
    FILE *fp;
    size_t got;
    int read_failed;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!verification || !out || !maximum_bytes) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_metadata_read",
                       "verified source, byte budget, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&blob, 0, sizeof(blob));
    rc = yvex_source_provenance_metadata_identity(
        verification, canonical_name, &blob.identity, err);
    if (rc != YVEX_OK) return rc;
    if (blob.identity.file_bytes > maximum_bytes ||
        blob.identity.file_bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "source_metadata_read",
                       "metadata sidecar exceeds its configured byte budget");
        return YVEX_ERR_BOUNDS;
    }
    blob.byte_count = (size_t)blob.identity.file_bytes;
    blob.bytes = (unsigned char *)malloc(blob.byte_count ? blob.byte_count : 1u);
    if (!blob.bytes) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_metadata_read",
                       "metadata sidecar buffer allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (!yvex_source_path_join(path, sizeof(path),
                               verification->resolved_source_path,
                               canonical_name)) {
        yvex_source_metadata_blob_release(&blob);
        yvex_error_set(err, YVEX_ERR_BOUNDS, "source_metadata_read",
                       "metadata sidecar path construction overflowed");
        return YVEX_ERR_BOUNDS;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        yvex_source_metadata_blob_release(&blob);
        yvex_error_set(err, YVEX_ERR_IO, "source_metadata_read",
                       "metadata sidecar open failed");
        return YVEX_ERR_IO;
    }
    got = blob.byte_count ? fread(blob.bytes, 1u, blob.byte_count, fp) : 0u;
    read_failed = got != blob.byte_count || ferror(fp);
    if (fclose(fp) != 0) read_failed = 1;
    if (read_failed) {
        yvex_source_metadata_blob_release(&blob);
        yvex_error_set(err, YVEX_ERR_IO, "source_metadata_read",
                       "metadata sidecar exact read failed");
        return YVEX_ERR_IO;
    }
    if (!source_git_blob_oid_bytes(blob.bytes, blob.byte_count,
                                   retained_oid) ||
        strcmp(retained_oid, blob.identity.expected_git_blob_oid) != 0) {
        yvex_source_metadata_blob_release(&blob);
        yvex_error_set(err, YVEX_ERR_FORMAT, "source_metadata_read",
                       "retained metadata bytes differ from provider identity");
        return YVEX_ERR_FORMAT;
    }
    memset(&after, 0, sizeof(after));
    rc = yvex_source_provenance_metadata_identity(
        verification, canonical_name, &after, err);
    if (rc != YVEX_OK ||
        after.file_bytes != blob.identity.file_bytes ||
        strcmp(after.observed_git_blob_oid,
               blob.identity.observed_git_blob_oid) != 0) {
        yvex_source_metadata_blob_release(&blob);
        if (rc == YVEX_OK)
            yvex_error_set(err, YVEX_ERR_FORMAT, "source_metadata_read",
                           "metadata sidecar identity drifted during read");
        return rc == YVEX_OK ? YVEX_ERR_FORMAT : rc;
    }
    *out = blob;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Releases one metadata blob and resets every observable fact. */
void yvex_source_metadata_blob_release(yvex_source_metadata_blob *blob)
{
    if (!blob) return;
    free(blob->bytes);
    memset(blob, 0, sizeof(*blob));
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
    int schema_v2;
    int schema_v3;
    int common;

    if (!options || !out) return 0;
    schema_v2 = strcmp(out->manifest_schema, "yvex.source_manifest.v2") == 0;
    schema_v3 = strcmp(out->manifest_schema, "yvex.source_manifest.v3") == 0;
    common = (schema_v2 || schema_v3) &&
           strcmp(out->manifest_status, "complete") == 0 &&
           strcmp(out->manifest_target_id, options->identity->target_id) == 0 &&
           strcmp(out->repository_id, options->identity->upstream_repo_id) == 0 &&
           strcmp(out->manifest_revision, out->revision) == 0 &&
           strcmp(out->inventory_authority,
                  options->identity->upstream_inventory_authority) == 0 &&
           strcmp(out->manifest_config_status, "verified") == 0 &&
           strcmp(out->manifest_tokenizer_status, "verified") == 0 &&
           strcmp(out->upstream_index_oid,
                  options->identity->upstream_index_oid
                      ? options->identity->upstream_index_oid
                      : "not-applicable") == 0 &&
           out->manifest_source_file_count == out->source_file_count &&
           out->manifest_source_total_bytes == out->source_total_bytes &&
           out->manifest_shard_count == out->shard_count &&
           out->manifest_shard_bytes == out->shard_bytes &&
           out->manifest_header_tensor_count == out->header_tensor_count;
    if (!common) return 0;
    if (schema_v2) {
        return strcmp(out->verification_stage,
                      "exact-source-metadata-header-verified") == 0 &&
               strcmp(out->manifest_payload_digest_status,
                      "not-verified") == 0;
    }
    return strcmp(out->verification_stage,
                  "exact-source-payload-verified") == 0 &&
           strcmp(out->manifest_payload_digest_status,
                  out->manifest_payload_trust_class) == 0 &&
           out->manifest_payload_shard_count == out->shard_count &&
           out->manifest_payload_bytes == out->shard_bytes &&
           out->manifest_payload_source_snapshot_identity ==
               out->source_snapshot_identity &&
           out->manifest_payload_tensor_count == out->header_tensor_count &&
           out->manifest_payload_logical_tensor_bytes ==
               out->declared_tensor_bytes;
}
