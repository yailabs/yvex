/*
 * write.c - source manifest JSON file writer.
 *
 * Owner: src/source.
 * Owns: explicit source sidecar file writing.
 * Does not own: CLI/operator output, report rendering, runtime, generation, eval, or benchmark.
 * Invariants: writes only caller-selected local files and never chooses standard streams.
 * Boundary: writing source sidecars is not source verification, artifact emission, or release readiness.
 */
#include "write.h"
#include "private.h"
#include "src/io/writer.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Writes only an unverified/in-progress source inventory manifest. */
int yvex_source_manifest_write_json(const char *out_path,
                                    const yvex_source_manifest_options *options,
                                    yvex_source_manifest_summary *summary_out,
                                    yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    if (!out_path || !options || !options->repo || !options->revision ||
        !options->local_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_write", "out_path, repo, revision, and local_path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->status == YVEX_SOURCE_STATUS_COMPLETE) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "source_manifest_write",
                       "complete manifests are published only by exact source verification");
        return YVEX_ERR_UNSUPPORTED;
    }

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(options->local_path, options->include_files, &files, err);
    if (rc == YVEX_OK) {
        rc = yvex_source_manifest_write_json_file(out_path, options, &files, err);
    }
    if (rc == YVEX_OK && summary_out) {
        *summary_out = files.summary;
    }
    yvex_source_manifest_file_list_free(&files);
    return rc;
}

static void json_field(FILE *fp, const char *name, const char *value, int comma)
{
    yvex_file_json_write_field(fp, "    ", name, value, comma);
}

/*
 * yvex_source_manifest_write_json_file()
 *
 * Purpose:
 *   write source manifest summary and file-list facts to JSON.
 *
 * Inputs:
 *   output path, manifest options, and file list are borrowed.
 *
 * Effects:
 *   opens a local JSON file, writes escaped metadata and footprint fields, and
 *   closes the file; it does not create artifacts or hash tensor payloads.
 *
 * Failure:
 *   returns invalid-arg or IO errors for missing inputs, open/write/close
 *   failures, and path issues.
 *
 * Boundary:
 *   writing a source manifest is not source verification, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err)
{
    FILE *fp;
    size_t i;

    if (!out_path || !options || !files) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_json", "out_path, options, and files are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "cannot open output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }

    fprintf(fp, "{\n");
    json_field(fp, "schema", "yvex.source_manifest.v1", 1);
    json_field(fp, "status", yvex_source_status_name(options->status), 1);
    fprintf(fp, "  \"source\": {\n");
    json_field(fp, "kind", "huggingface", 1);
    json_field(fp, "repo", options->repo, 1);
    json_field(fp, "revision", options->revision, 1);
    json_field(fp, "license", options->license, 1);
    json_field(fp, "model_card", options->model_card, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"local\": {\n");
    json_field(fp, "path", options->local_path, 1);
    json_field(fp, "node_role", "provider", 1);
    json_field(fp, "node_name", options->node_name, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"download\": {\n");
    json_field(fp, "command", options->download_command, 1);
    json_field(fp, "dry_run_log", options->dry_run_log, 1);
    json_field(fp, "download_log", options->download_log, 1);
    json_field(fp, "pid_file", options->pid_file, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"files\": [\n");
    for (i = 0; i < files->count; ++i) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"path\": ");
        yvex_file_json_write_string(fp, files->items[i].path);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"size_bytes\": %llu,\n", files->items[i].size_bytes);
        fprintf(fp, "      \"kind\": ");
        yvex_file_json_write_string(fp, files->items[i].kind);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"sha256\": null\n");
        fprintf(fp, "    }%s\n", i + 1u == files->count ? "" : ",");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"summary\": {\n");
    fprintf(fp, "    \"file_count\": %llu,\n", files->summary.file_count);
    fprintf(fp, "    \"safetensors_count\": %llu,\n", files->summary.safetensors_count);
    fprintf(fp, "    \"total_size_bytes\": %llu,\n", files->summary.total_size_bytes);
    fprintf(fp, "    \"has_config\": %s,\n", files->summary.has_config ? "true" : "false");
    fprintf(fp, "    \"has_tokenizer\": %s,\n", files->summary.has_tokenizer ? "true" : "false");
    fprintf(fp, "    \"has_safetensors\": %s\n", files->summary.has_safetensors ? "true" : "false");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "failed closing output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

typedef int (*source_atomic_writer)(FILE *fp, const void *context);

/* Publishes one same-directory temporary file through fsync and atomic rename. */
static int source_publish_atomic(const char *out_path,
                                 source_atomic_writer writer,
                                 const void *context,
                                 yvex_error *err)
{
    char temporary[YVEX_PATH_CAP];
    int fd = -1;
    FILE *fp = NULL;
    int n;
    int rc = YVEX_ERR_IO;

    if (!out_path || !writer) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_publish_atomic",
                       "output path and writer are required");
        return YVEX_ERR_INVALID_ARG;
    }
    n = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", out_path,
                 (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "source_publish_atomic",
                       "temporary manifest path is too long");
        return YVEX_ERR_BOUNDS;
    }
    fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_publish_atomic",
                        "cannot create temporary output: %s", temporary);
        return YVEX_ERR_IO;
    }
    fp = fdopen(fd, "wb");
    if (!fp) {
        close(fd);
        unlink(temporary);
        yvex_error_setf(err, YVEX_ERR_IO, "source_publish_atomic",
                        "cannot open temporary output stream: %s", temporary);
        return YVEX_ERR_IO;
    }
    fd = -1;
    if (!writer(fp, context) || fflush(fp) != 0 ||
        getenv("YVEX_TEST_FAIL_SOURCE_PUBLISH_AFTER_WRITE")) {
        yvex_error_set(err, YVEX_ERR_IO, "source_publish_atomic",
                       "temporary output write failed");
        goto cleanup;
    }
    if (fsync(fileno(fp)) != 0) {
        yvex_error_set(err, YVEX_ERR_IO, "source_publish_atomic",
                       "temporary output fsync failed");
        goto cleanup;
    }
    if (fclose(fp) != 0) {
        fp = NULL;
        yvex_error_set(err, YVEX_ERR_IO, "source_publish_atomic",
                       "temporary output close failed");
        goto cleanup;
    }
    fp = NULL;
    if (rename(temporary, out_path) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_publish_atomic",
                        "atomic output rename failed: %s", strerror(errno));
        goto cleanup;
    }
    rc = YVEX_OK;
cleanup:
    if (fp) fclose(fp);
    if (rc != YVEX_OK) unlink(temporary);
    return rc;
}

typedef struct {
    const yvex_source_verify_options *options;
    const yvex_source_verification *verification;
} source_verified_manifest_context;

/* Serializes already-approved exact verification facts to a temporary stream. */
static int source_write_verified_manifest(FILE *fp, const void *opaque)
{
    const source_verified_manifest_context *context =
        (const source_verified_manifest_context *)opaque;
    const yvex_source_verify_options *options = context->options;
    const yvex_source_verification *verification = context->verification;

    if (fprintf(fp, "{\n  \"schema\": \"yvex.source_manifest.v2\",\n"
                    "  \"status\": \"complete\",\n"
                    "  \"target\": {\"id\": ") < 0) return 0;
    yvex_file_json_write_string(fp, options->identity->target_id);
    if (fprintf(fp, ", \"family\": ") < 0) return 0;
    yvex_file_json_write_string(fp, options->identity->family_key);
    if (fprintf(fp, "},\n  \"source\": {\"kind\": \"huggingface\", "
                    "\"repo\": ") < 0) return 0;
    yvex_file_json_write_string(fp, options->identity->upstream_repo_id);
    if (fprintf(fp, ", \"revision\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->revision);
    if (fprintf(fp, "},\n  \"local\": {\"path\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->resolved_source_path);
    if (fprintf(fp,
                "},\n  \"verification\": {\n"
                "    \"stage\": \"exact-source-metadata-header-verified\",\n"
                "    \"inventory_authority\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->inventory_authority);
    if (fprintf(fp,
                ",\n    \"upstream_index_oid\": ") < 0) return 0;
    yvex_file_json_write_string(
        fp, verification->upstream_index_oid[0]
                ? verification->upstream_index_oid : "not-applicable");
    return fprintf(fp,
                   ",\n    \"source_file_count\": %llu,\n"
                   "    \"source_total_bytes\": %llu,\n"
                   "    \"shard_count\": %llu,\n"
                   "    \"shard_bytes\": %llu,\n"
                   "    \"header_tensor_count\": %llu,\n"
                   "    \"config_status\": \"verified\",\n"
                   "    \"tokenizer_status\": \"verified\",\n"
                   "    \"payload_digest_status\": \"not-verified\"\n"
                   "  }\n}\n",
                   verification->source_file_count,
                   verification->source_total_bytes,
                   verification->shard_count,
                   verification->shard_bytes,
                   verification->header_tensor_count) >= 0;
}

/* Admits only blocker-free exact facts to atomic complete-manifest publication. */
int yvex_source_manifest_publish_verified(
    const char *out_path,
    const yvex_source_verify_options *options,
    const yvex_source_verification *verification,
    yvex_error *err)
{
    source_verified_manifest_context context;

    if (!out_path || !options || !options->identity || !verification ||
        !verification->path_verified || !verification->repository_verified ||
        !verification->revision_verified || !verification->config_valid ||
        !verification->tokenizer_json_valid ||
        !verification->tokenizer_config_valid ||
        !verification->generation_config_valid ||
        !verification->shard_index_headers_match ||
        verification->header_scan_count != 1u ||
        (strcmp(verification->inventory_authority, "upstream-index") == 0 &&
         !verification->upstream_index_identity_verified) ||
        (strcmp(verification->inventory_authority, "upstream-index") != 0 &&
         strcmp(verification->inventory_authority, "header-derived") != 0) ||
        verification->blocker_count != 0u) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED,
                       "source_manifest_publish_verified",
                       "only blocker-free exact verifier facts may publish a complete manifest");
        return YVEX_ERR_UNSUPPORTED;
    }
    context.options = options;
    context.verification = verification;
    return source_publish_atomic(out_path, source_write_verified_manifest,
                                 &context, err);
}

typedef struct {
    const yvex_source_verification *verification;
    const yvex_source_payload_session *session;
} source_payload_manifest_context;

/* Serializes a fully trusted payload set; raw payload bytes never enter output. */
static int source_write_payload_manifest(FILE *fp, const void *opaque)
{
    const source_payload_manifest_context *context =
        (const source_payload_manifest_context *)opaque;
    const yvex_source_verification *verification = context->verification;
    const yvex_source_payload_session *session = context->session;
    unsigned long long index;

    if (fprintf(fp, "{\n  \"schema\": \"yvex.source_manifest.v3\",\n"
                    "  \"status\": \"complete\",\n"
                    "  \"target\": {\"id\": ") < 0) return 0;
    yvex_file_json_write_string(fp, session->target_id);
    if (fprintf(fp, ", \"family\": ") < 0) return 0;
    yvex_file_json_write_string(fp, session->family_key);
    if (fprintf(fp, "},\n  \"source\": {\"kind\": \"huggingface\", \"repo\": ") < 0)
        return 0;
    yvex_file_json_write_string(fp, session->repository_id);
    if (fprintf(fp, ", \"revision\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->revision);
    if (fprintf(fp, "},\n  \"local\": {\"path\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->resolved_source_path);
    if (fprintf(fp,
                "},\n  \"verification\": {\n"
                "    \"stage\": \"exact-source-payload-verified\",\n"
                "    \"inventory_authority\": ") < 0) return 0;
    yvex_file_json_write_string(fp, verification->inventory_authority);
    if (fprintf(fp, ",\n    \"upstream_index_oid\": ") < 0) return 0;
    yvex_file_json_write_string(
        fp, verification->upstream_index_oid[0]
                ? verification->upstream_index_oid : "not-applicable");
    if (fprintf(fp,
                ",\n    \"source_file_count\": %llu,\n"
                "    \"source_total_bytes\": %llu,\n"
                "    \"shard_count\": %llu,\n"
                "    \"shard_bytes\": %llu,\n"
                "    \"header_tensor_count\": %llu,\n"
                "    \"config_status\": \"verified\",\n"
                "    \"tokenizer_status\": \"verified\",\n"
                "    \"payload_digest_status\": ",
                verification->source_file_count,
                verification->source_total_bytes,
                verification->shard_count,
                verification->shard_bytes,
                verification->header_tensor_count) < 0) return 0;
    yvex_file_json_write_string(
        fp, yvex_source_payload_trust_class_name(session->facts.trust_class));
    if (fprintf(fp,
                "\n  },\n  \"payload\": {\n"
                "    \"identity\": ") < 0) return 0;
    yvex_file_json_write_string(fp, session->facts.payload_identity);
    if (fprintf(fp, ",\n    \"trust_class\": ") < 0) return 0;
    yvex_file_json_write_string(
        fp, yvex_source_payload_trust_class_name(session->facts.trust_class));
    if (fprintf(fp,
                ",\n    \"digest_algorithm\": \"sha256\",\n"
                "    \"source_snapshot_identity\": %llu,\n"
                "    \"shard_count\": %llu,\n"
                "    \"shard_file_bytes\": %llu,\n"
                "    \"tensor_count\": %llu,\n"
                "    \"logical_tensor_bytes\": %llu,\n"
                "    \"shards\": [\n",
                session->facts.source_snapshot_identity,
                session->facts.shard_count,
                verification->shard_bytes,
                session->facts.tensor_count,
                session->facts.logical_tensor_bytes) < 0) return 0;
    for (index = 0u; index < session->shard_count; ++index) {
        const yvex_source_payload_owned_shard *shard = &session->shards[index];

        if (fprintf(fp, "      {\"id\": %llu, \"name\": ", index) < 0)
            return 0;
        yvex_file_json_write_string(fp, shard->name);
        if (fprintf(fp,
                    ", \"file_bytes\": %llu, \"data_region_offset\": %llu, "
                    "\"payload_bytes\": %llu, \"digest_algorithm\": ",
                    shard->public_fact.file_bytes,
                    shard->public_fact.data_region_offset,
                    shard->public_fact.payload_bytes) < 0) return 0;
        yvex_file_json_write_string(fp, shard->digest_algorithm);
        if (fprintf(fp, ", \"digest_authority\": ") < 0) return 0;
        yvex_file_json_write_string(fp, shard->digest_authority);
        if (fprintf(fp, ", \"expected_digest\": ") < 0) return 0;
        if (shard->expected_digest[0])
            yvex_file_json_write_string(fp, shard->expected_digest);
        else if (fprintf(fp, "null") < 0)
            return 0;
        if (fprintf(fp, ", \"observed_digest\": ") < 0) return 0;
        yvex_file_json_write_string(fp, shard->observed_digest);
        if (fprintf(fp, ", \"trust_class\": ") < 0) return 0;
        yvex_file_json_write_string(
            fp, yvex_source_payload_trust_class_name(
                    shard->public_fact.trust_class));
        if (fprintf(fp, "}%s\n",
                    index + 1u == session->shard_count ? "" : ",") < 0)
            return 0;
    }
    return fprintf(fp, "    ]\n  }\n}\n") >= 0;
}

/* Publishes v3 only after all digests and aggregate identity have completed. */
int yvex_source_manifest_publish_payload(
    const char *out_path,
    const yvex_source_verification *verification,
    const yvex_source_payload_session *session,
    yvex_error *err)
{
    source_payload_manifest_context context;

    if (!out_path || !verification ||
        !verification->verified || !session ||
        session->state != YVEX_SOURCE_PAYLOAD_STATE_VERIFYING ||
        session->facts.trust_class == YVEX_SOURCE_PAYLOAD_TRUST_NONE ||
        session->facts.trusted_shard_count != session->shard_count ||
        !session->facts.payload_identity[0]) {
        yvex_error_set(err, YVEX_ERR_STATE,
                       "source_manifest_publish_payload",
                       "only complete trusted payload facts may publish a v3 manifest");
        return YVEX_ERR_STATE;
    }
    context.verification = verification;
    context.session = session;
    return source_publish_atomic(out_path, source_write_payload_manifest,
                                 &context, err);
}

typedef struct {
    const yvex_source_verify_options *options;
    const yvex_source_derived_inventory *inventory;
} source_derived_inventory_context;

/* Serializes deterministic header-derived rows with explicit YVEX authority. */
static int source_write_derived_inventory(FILE *fp, const void *opaque)
{
    const source_derived_inventory_context *context =
        (const source_derived_inventory_context *)opaque;
    size_t i;

    if (fprintf(fp,
                "{\n  \"schema\": \"yvex.source_header_inventory.v1\",\n"
                "  \"authority\": \"yvex-header-derived\",\n"
                "  \"repository\": ") < 0) return 0;
    yvex_file_json_write_string(fp, context->options->identity->upstream_repo_id);
    if (fprintf(fp, ",\n  \"revision\": ") < 0) return 0;
    yvex_file_json_write_string(fp, context->options->identity->upstream_revision);
    if (fprintf(fp, ",\n  \"weight_map\": {\n") < 0) return 0;
    for (i = 0u; i < context->inventory->count; ++i) {
        if (fprintf(fp, "    ") < 0) return 0;
        yvex_file_json_write_string(fp, context->inventory->rows[i].tensor);
        if (fprintf(fp, ": ") < 0) return 0;
        yvex_file_json_write_string(fp, context->inventory->rows[i].shard);
        if (fprintf(fp, "%s\n",
                    i + 1u == context->inventory->count ? "" : ",") < 0) {
            return 0;
        }
    }
    return fprintf(fp, "  }\n}\n") >= 0;
}

/* Atomically publishes a nonempty derived inventory outside the source tree. */
int yvex_source_derived_inventory_publish(
    const char *out_path,
    const yvex_source_verify_options *options,
    const yvex_source_derived_inventory *inventory,
    yvex_error *err)
{
    source_derived_inventory_context context;

    if (!out_path || !options || !inventory || !inventory->rows ||
        inventory->count == 0u ||
        strcmp(options->identity->upstream_inventory_authority,
               "header-derived") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "source_derived_inventory_publish",
                       "header-derived inventory output and rows are required");
        return YVEX_ERR_INVALID_ARG;
    }
    context.options = options;
    context.inventory = inventory;
    return source_publish_atomic(out_path, source_write_derived_inventory,
                                 &context, err);
}
