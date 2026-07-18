/*
 * Owner: gguf.naming (gguf).
 * Owns: the reusable-algorithm boundary consumed by artifact,model,quant.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: reusable-algorithm; moving this contract requires an ownership-manifest change.
 *
 * gguf/naming.c - YVEX artifact naming helpers.
 *
 * This file owns deterministic artifact filename construction and validation.
 */

#include <yvex/artifact_naming.h>

#include <stdio.h>
#include <string.h>

static int is_empty(const char *s)
{
    return !s || s[0] == '\0';
}

static int contains_space(const char *s)
{
    while (s && *s) {
        if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') return 1;
        ++s;
    }
    return 0;
}

static int has_ambiguous_word(const char *s)
{
    return s && (strstr(s, "test") ||
                 strstr(s, "final") ||
                 strstr(s, "new") ||
                 strstr(s, "fixed") ||
                 strstr(s, "latest"));
}

static int validate_part(const char *part, const char *name, yvex_error *err)
{
    if (is_empty(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                        "%s is required", name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (contains_space(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                        "%s must not contain spaces", name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (has_ambiguous_word(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                        "%s contains ambiguous naming vocabulary", name);
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

int yvex_artifact_name_suggest(char *out,
                               size_t out_size,
                               const char *family,
                               const char *model,
                               const char *scope,
                               const char *artifact_class,
                               const char *qprofile,
                               const char *calibration,
                               const char *producer,
                               const char *schema,
                               yvex_error *err)
{
    int n;
    int rc;

    if (!out || out_size == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "output buffer is required");
        return YVEX_ERR_INVALID_ARG;
    }
    out[0] = '\0';
    rc = validate_part(family, "family", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(model, "model", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(scope, "scope", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(artifact_class, "artifact_class", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(qprofile, "qprofile", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(calibration, "calibration", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(producer, "producer", err);
    if (rc != YVEX_OK) return rc;
    rc = validate_part(schema, "schema", err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(producer, "yvex") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "producer must be yvex");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(schema, "v1") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "schema must be v1");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(out, out_size, "%s-%s-%s-%s-%s-%s-%s-%s.gguf",
                 family, model, scope, artifact_class, qprofile, calibration,
                 producer, schema);
    if (n < 0 || (size_t)n >= out_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_name_suggest",
                       "artifact filename buffer too small");
        out[0] = '\0';
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}
