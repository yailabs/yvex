/*
 * yvex_source_verify.c - exact local source verification coordinator.
 *
 * Owner: src/source.
 * Owns: final verification policy, blocker facts, phase coordination, and
 *   verifier-controlled manifest promotion.
 * Does not own: JSON parsing, DeepSeek metadata parsing, provenance parsing,
 *   shard/header inventory, file serialization, rendering, or payload reads.
 * Invariants: each specialized owner is consumed once; strict success requires
 *   a reopened complete manifest and one canonical header scan.
 * Boundary: exact source verification is not artifact, runtime, or generation support.
 */
#define _XOPEN_SOURCE 700
#include "yvex_source_verify.h"

#include "yvex_source_deepseek.h"
#include "yvex_source_inventory.h"
#include "yvex_source_json.h"
#include "yvex_source_private.h"
#include "yvex_source_provenance.h"
#include "yvex_source_verify_internal.h"
#include "yvex_source_write.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SOURCE_CONFIG_CAP (1024u * 1024u)
#define SOURCE_TOKENIZER_CAP (32u * 1024u * 1024u)

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
    yvex_source_deepseek_sidecar_kind kind)
{
    switch (kind) {
    case YVEX_SOURCE_DEEPSEEK_CONFIG: return "config.json";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER: return "tokenizer.json";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER_CONFIG: return "tokenizer_config.json";
    case YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG: return "generation_config.json";
    default: return "";
    }
}

static const char *source_sidecar_missing(
    yvex_source_deepseek_sidecar_kind kind)
{
    switch (kind) {
    case YVEX_SOURCE_DEEPSEEK_CONFIG: return "missing-source-config";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER: return "missing-tokenizer-json";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER_CONFIG: return "missing-tokenizer-config";
    case YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG: return "missing-generation-config";
    default: return "missing-source-sidecar";
    }
}

static const char *source_sidecar_malformed(
    yvex_source_deepseek_sidecar_kind kind)
{
    switch (kind) {
    case YVEX_SOURCE_DEEPSEEK_CONFIG: return "malformed-source-config";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER: return "malformed-tokenizer-json";
    case YVEX_SOURCE_DEEPSEEK_TOKENIZER_CONFIG: return "malformed-tokenizer-config";
    case YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG: return "malformed-generation-config";
    default: return "malformed-source-sidecar";
    }
}

/* Reads, structurally parses, and provenance-checks one required sidecar. */
static int source_verify_sidecar(
    const yvex_source_verify_options *options,
    yvex_source_deepseek_sidecar_kind kind,
    yvex_source_verification *out,
    yvex_error *err)
{
    const char *name = source_sidecar_name(kind);
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    size_t cap = kind == YVEX_SOURCE_DEEPSEEK_TOKENIZER
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
    if (!yvex_source_deepseek_parse_sidecar(
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
    for (kind = YVEX_SOURCE_DEEPSEEK_CONFIG;
         kind <= YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG; ++kind) {
        rc = source_verify_sidecar(
            options, (yvex_source_deepseek_sidecar_kind)kind, out, err);
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
