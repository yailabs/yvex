/*
 * sha256.h - private incremental SHA-256 primitive.
 *
 * Owner: src/core.
 * Owns: SHA-256 state, incremental update, finalization, and hexadecimal form.
 * Does not own: files, provenance policy, manifests, payload sessions, or output.
 * Invariants: input is borrowed; finalization writes exactly 32 digest bytes.
 * Boundary: a digest primitive alone establishes neither provenance nor trust.
 */
#ifndef YVEX_SHA256_PRIVATE_H
#define YVEX_SHA256_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#define YVEX_SHA256_DIGEST_BYTES 32u
#define YVEX_SHA256_HEX_BYTES 65u

typedef struct {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
    int finalized;
} yvex_sha256;

void yvex_sha256_init(yvex_sha256 *context);
int yvex_sha256_update(yvex_sha256 *context, const void *data, size_t length);
int yvex_sha256_final(yvex_sha256 *context,
                      unsigned char digest[YVEX_SHA256_DIGEST_BYTES]);
void yvex_sha256_hex(const unsigned char digest[YVEX_SHA256_DIGEST_BYTES],
                     char output[YVEX_SHA256_HEX_BYTES]);
int yvex_sha256_hex_valid(const char *text);

#endif
