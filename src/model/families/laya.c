/* Bind one immutable published Laya checkpoint to its exact family semantics. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/families/laya_source.h>
#include <yvex/internal/core.h>
#include <yvex/internal/source.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { const char *path, *digest; } laya_source_file;
static const laya_source_file source_files[] = {
    {"rl_agent_config.json", "ebf0cd524d92342a6be5e48e9fca3d7c2babfb5a56ccd79d2171ef5d8c7f7be8"},
    {"encoder/config.json", "5268d24ad3b77c8151de5dcb0762ba4391619aad9ab0bda33e36fb083cfeae6d"},
    {"tokenizer/tokenizer.json", "6c8aaa9a542084f2457eab775d4eeb51f92a70c0fd9de28d5edb0ddec3c08d30"},
    {"tokenizer/tokenizer_config.json", "08d4cf3ac4dca381759441b85b91a6d40e688471dcd33d15d6649eb0a9a854d1"}
};
static const char *weight_digest =
    "4fa56de72383a9d3efa9cfa78955733c81b9fc8067a587ca4beb82c78107a24e";
static const char *source_revision = "1a793eb568e6718f15941d08f85432581df534e3";

static int laya_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "model.laya.source", reason);
    return status;
}

static int source_path(char *out, size_t capacity, const char *root, const char *relative,
    yvex_error *err)
{
    if (!root || !*root || !relative || snprintf(out, capacity, "%s/%s", root, relative) >=
        (int)capacity)
        return laya_refuse(err, YVEX_ERR_BOUNDS, "checkpoint source path exceeds bound");
    return YVEX_OK;
}

static int source_file_verify(const char *path, const char *expected,
    unsigned long long expected_bytes, yvex_error *err)
{
    unsigned char buffer[65536], digest[YVEX_SHA256_DIGEST_BYTES];
    char observed[YVEX_SHA256_HEX_BYTES];
    struct stat statbuf;
    yvex_sha256 hash;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 || fstat(fd, &statbuf) != 0 || !S_ISREG(statbuf.st_mode) ||
        statbuf.st_size < 1 ||
        (expected_bytes && (unsigned long long)statbuf.st_size != expected_bytes) ||
        (!expected_bytes && statbuf.st_size > 8 * 1024 * 1024)) {
        if (fd >= 0) close(fd);
        return laya_refuse(err, YVEX_ERR_IO, "bounded regular checkpoint sidecar required");
    }
    yvex_sha256_init(&hash);
    ssize_t nread;
    for (;;) {
        nread = read(fd, buffer, sizeof(buffer));
        if (nread < 0 && errno == EINTR) continue;
        if (nread <= 0 || !yvex_sha256_update(&hash, buffer, (size_t)nread)) break;
    }
    close(fd);
    if (nread != 0 || !yvex_sha256_final(&hash, digest))
        return laya_refuse(err, YVEX_ERR_IO, "checkpoint sidecar hash failed");
    yvex_sha256_hex(digest, observed);
    if (strcmp(observed, expected))
        return laya_refuse(err, YVEX_ERR_FORMAT, "checkpoint sidecar differs from immutable revision");
    return YVEX_OK;
}

int yvex_laya_typed_source_admit(const char *checkpoint_directory,
    yvex_laya_typed_source_summary *summary, yvex_error *err)
{
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!checkpoint_directory || !summary)
        return laya_refuse(err, YVEX_ERR_INVALID_ARG, "checkpoint and typed summary required");
    char path[1024];
    for (size_t i = 0u; i < sizeof(source_files) / sizeof(source_files[0]); ++i) {
        if (source_path(path, sizeof(path), checkpoint_directory, source_files[i].path, err) != YVEX_OK)
            return yvex_error_code(err);
        int rc = source_file_verify(path, source_files[i].digest, 0u, err);
        if (rc != YVEX_OK) return rc;
    }
    if (source_path(path, sizeof(path), checkpoint_directory, "model.safetensors", err) != YVEX_OK)
        return yvex_error_code(err);
    int rc = source_file_verify(path, weight_digest, 842609220ull, err);
    if (rc != YVEX_OK) return rc;
    yvex_native_weight_table *weights = calloc(1u, sizeof(*weights));
    if (!weights) return laya_refuse(err, YVEX_ERR_NOMEM, "tensor directory allocation failed");
    yvex_safetensors_file_facts facts = {0};
    rc = yvex_safetensors_read_header_file_with_facts(path, "weights", weights, &facts, err);
    if (rc == YVEX_OK) rc = yvex_native_weight_table_finalize(weights, err);
    if (rc == YVEX_OK && (facts.file_bytes != 842609220ull ||
        yvex_native_weight_table_count(weights) != 206u))
        rc = laya_refuse(err, YVEX_ERR_FORMAT, "checkpoint tensor inventory differs from release");
    yvex_native_weight_table_close(weights);
    if (rc != YVEX_OK) return rc;
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    int sealed = yvex_sha256_update_text(&hash, "yvex.model.laya-typed-source.v1") &&
        yvex_sha256_update_text(&hash, "convaiinnovations/laya-typed-decisions") &&
        yvex_sha256_update_text(&hash, source_revision) &&
        yvex_sha256_update_text(&hash, weight_digest);
    for (size_t i = 0u; i < sizeof(source_files) / sizeof(source_files[0]); ++i)
        sealed = sealed && yvex_sha256_update_text(&hash, source_files[i].path) &&
            yvex_sha256_update_text(&hash, source_files[i].digest);
    sealed = sealed && yvex_sha256_final(&hash, digest);
    if (!sealed) {
        return laya_refuse(err, YVEX_ERR_STATE, "logical source identity unavailable");
    }
    summary->schema_version = YVEX_LAYA_TYPED_SOURCE_SCHEMA_V1;
    yvex_core_text_copy(summary->repository, sizeof(summary->repository),
        "convaiinnovations/laya-typed-decisions");
    yvex_core_text_copy(summary->revision, sizeof(summary->revision), source_revision);
    yvex_core_text_copy(summary->weight_path, sizeof(summary->weight_path), path);
    yvex_core_text_copy(summary->weight_identity, sizeof(summary->weight_identity), weight_digest);
    yvex_core_text_copy(summary->tokenizer_identity, sizeof(summary->tokenizer_identity),
        source_files[2].digest);
    yvex_sha256_hex(digest, summary->logical_model_identity);
    summary->weight_bytes = facts.file_bytes;
    summary->weight_tensor_count = 206u;
    summary->maximum_tokens = 64u;
    summary->layer_count = 28u;
    summary->vocabulary_size = 50368u;
    summary->hidden_width = 1024u;
    summary->attention_heads = 16u;
    summary->intermediate_width = 2624u;
    summary->head_layer_count = 2u;
    summary->epsilon = 1e-5;
    yvex_error_clear(err);
    return YVEX_OK;
}
