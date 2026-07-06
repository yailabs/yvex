/*
 * yvex_artifact_identity.c - Local artifact file identity helpers.
 *
 * This module computes file size and SHA-256 for operator-local artifacts. The
 * digest is local identity evidence only; it does not prove provenance,
 * authorship, or supply-chain trust.
 */

#include <yvex/artifact_identity.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char block[64];
    unsigned int used;
} yvex_sha256_ctx;

static uint32_t rotr32(uint32_t v, unsigned int n)
{
    return (v >> n) | (v << (32u - n));
}

static void sha256_init(yvex_sha256_ctx *ctx)
{
    ctx->h[0] = 0x6a09e667u;
    ctx->h[1] = 0xbb67ae85u;
    ctx->h[2] = 0x3c6ef372u;
    ctx->h[3] = 0xa54ff53au;
    ctx->h[4] = 0x510e527fu;
    ctx->h[5] = 0x9b05688cu;
    ctx->h[6] = 0x1f83d9abu;
    ctx->h[7] = 0x5be0cd19u;
    ctx->len = 0;
    ctx->used = 0;
}

static void sha256_transform(yvex_sha256_ctx *ctx, const unsigned char block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned int i;

    for (i = 0; i < 16u; ++i) {
        w[i] = ((uint32_t)block[i * 4u] << 24) |
               ((uint32_t)block[i * 4u + 1u] << 16) |
               ((uint32_t)block[i * 4u + 2u] << 8) |
               ((uint32_t)block[i * 4u + 3u]);
    }
    for (i = 16u; i < 64u; ++i) {
        uint32_t s0 = rotr32(w[i - 15u], 7u) ^ rotr32(w[i - 15u], 18u) ^ (w[i - 15u] >> 3);
        uint32_t s1 = rotr32(w[i - 2u], 17u) ^ rotr32(w[i - 2u], 19u) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3];
    e = ctx->h[4]; f = ctx->h[5]; g = ctx->h[6]; h = ctx->h[7];
    for (i = 0; i < 64u; ++i) {
        uint32_t s1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void sha256_update(yvex_sha256_ctx *ctx, const unsigned char *data, unsigned long long len)
{
    while (len > 0ull) {
        unsigned int take = 64u - ctx->used;
        if ((unsigned long long)take > len) {
            take = (unsigned int)len;
        }
        memcpy(ctx->block + ctx->used, data, take);
        ctx->used += take;
        data += take;
        len -= take;
        ctx->len += take;
        if (ctx->used == 64u) {
            sha256_transform(ctx, ctx->block);
            ctx->used = 0;
        }
    }
}

static void sha256_final(yvex_sha256_ctx *ctx, unsigned char out[32])
{
    uint64_t bit_len = ctx->len * 8ull;
    unsigned int i;

    ctx->block[ctx->used++] = 0x80u;
    if (ctx->used > 56u) {
        while (ctx->used < 64u) {
            ctx->block[ctx->used++] = 0u;
        }
        sha256_transform(ctx, ctx->block);
        ctx->used = 0;
    }
    while (ctx->used < 56u) {
        ctx->block[ctx->used++] = 0u;
    }
    for (i = 0; i < 8u; ++i) {
        ctx->block[63u - i] = (unsigned char)((bit_len >> (i * 8u)) & 0xffu);
    }
    sha256_transform(ctx, ctx->block);
    for (i = 0; i < 8u; ++i) {
        out[i * 4u] = (unsigned char)((ctx->h[i] >> 24) & 0xffu);
        out[i * 4u + 1u] = (unsigned char)((ctx->h[i] >> 16) & 0xffu);
        out[i * 4u + 2u] = (unsigned char)((ctx->h[i] >> 8) & 0xffu);
        out[i * 4u + 3u] = (unsigned char)(ctx->h[i] & 0xffu);
    }
}

static void digest_to_hex(const unsigned char digest[32], char out_hex[YVEX_SHA256_HEX_CAP])
{
    static const char hex[] = "0123456789abcdef";
    unsigned int i;

    for (i = 0; i < 32u; ++i) {
        out_hex[i * 2u] = hex[(digest[i] >> 4) & 0x0fu];
        out_hex[i * 2u + 1u] = hex[digest[i] & 0x0fu];
    }
    out_hex[64] = '\0';
}

int yvex_artifact_sha256_hex_bytes(const unsigned char *data,
                                   unsigned long long len,
                                   char out_hex[YVEX_SHA256_HEX_CAP],
                                   yvex_error *err)
{
    yvex_sha256_ctx ctx;
    unsigned char digest[32];

    if (!data || !out_hex) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_sha256_hex_bytes",
                       "data and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
    digest_to_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_compute_sha256(const char *path,
                                 char out_hex[YVEX_SHA256_HEX_CAP],
                                 yvex_error *err)
{
    FILE *fp;
    yvex_sha256_ctx ctx;
    unsigned char digest[32];
    unsigned char buf[65536];

    if (!path || !path[0] || !out_hex) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_compute_sha256",
                       "path and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_compute_sha256",
                        "cannot open artifact for sha256: %s", path);
        return YVEX_ERR_IO;
    }

    sha256_init(&ctx);
    for (;;) {
        size_t n = fread(buf, 1u, sizeof(buf), fp);
        if (n > 0u) {
            sha256_update(&ctx, buf, (unsigned long long)n);
        }
        if (n < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_compute_sha256",
                                "cannot read artifact for sha256: %s", path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    sha256_final(&ctx, digest);
    digest_to_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_identity_read(const char *path,
                                yvex_artifact_file_identity *out,
                                yvex_error *err)
{
    FILE *fp;
    yvex_sha256_ctx ctx;
    unsigned char digest[32];
    unsigned char buf[65536];
    unsigned long long size = 0ull;
    int n;

    if (!path || !path[0] || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_identity_read",
                       "path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    n = snprintf(out->path, sizeof(out->path), "%s", path);
    if (n < 0 || (unsigned int)n >= sizeof(out->path)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read",
                       "artifact path too long");
        return YVEX_ERR_BOUNDS;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_identity_read",
                        "cannot open artifact for identity: %s", path);
        return YVEX_ERR_IO;
    }

    sha256_init(&ctx);
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), fp);
        if (got > 0u) {
            sha256_update(&ctx, buf, (unsigned long long)got);
            if (ULLONG_MAX - size < (unsigned long long)got) {
                fclose(fp);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read",
                               "artifact file size overflow");
                return YVEX_ERR_BOUNDS;
            }
            size += (unsigned long long)got;
        }
        if (got < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_identity_read",
                                "cannot read artifact for identity: %s", path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    sha256_final(&ctx, digest);
    digest_to_hex(digest, out->sha256);
    out->file_size = size;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_sha256_hex_is_valid(const char *hex)
{
    unsigned int i;

    if (!hex) {
        return 0;
    }
    for (i = 0; i < 64u; ++i) {
        if (!hex[i] || !isxdigit((unsigned char)hex[i])) {
            return 0;
        }
    }
    return hex[64] == '\0';
}
