/*
 * YVEX - Local artifact identity
 *
 * File: include/yvex/artifact_identity.h
 * Layer: public artifact API
 *
 * Purpose:
 *   Defines local file identity helpers used by the model registry, integrity
 *   checks, and gates. Identity is local evidence: file size plus SHA-256 over
 *   the current bytes. It is not supply-chain security or provenance.
 */
#ifndef YVEX_ARTIFACT_IDENTITY_H
#define YVEX_ARTIFACT_IDENTITY_H

#include <yvex/artifact.h>
#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_SHA256_HEX_CAP 65u

typedef struct {
    char path[YVEX_ARTIFACT_PATH_CAP];
    unsigned long long file_size;
    char sha256[YVEX_SHA256_HEX_CAP];
} yvex_artifact_file_identity;

int yvex_artifact_compute_sha256(const char *path,
                                 char out_hex[YVEX_SHA256_HEX_CAP],
                                 yvex_error *err);

int yvex_artifact_identity_read(const char *path,
                                yvex_artifact_file_identity *out,
                                yvex_error *err);

/* Hashes the exact already-opened artifact snapshot through positioned reads. */
int yvex_artifact_identity_read_open(const yvex_artifact *artifact,
                                     yvex_artifact_file_identity *out,
                                     yvex_error *err);

int yvex_artifact_sha256_hex_bytes(const unsigned char *data,
                                   unsigned long long len,
                                   char out_hex[YVEX_SHA256_HEX_CAP],
                                   yvex_error *err);

int yvex_sha256_hex_is_valid(const char *hex);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_IDENTITY_H */
