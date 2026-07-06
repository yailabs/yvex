/*
 * yvex_profile.c - Runtime profile document boundary.
 *
 * This file owns the canonical source boundary for profile output and runtime
 * profile documents. It does not create benchmark numbers or generation
 * readiness claims.
 */

#include <yvex/profile.h>

#include <stdio.h>

static int profile_open_output(FILE **out, const char *path, yvex_error *err)
{
    if (!out || !path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_profile_write_json",
                       "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    *out = fopen(path, "w");
    if (!*out) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_profile_write_json",
                        "cannot open output file %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int profile_json_write_string(FILE *fp, const char *text)
{
    const unsigned char *p;

    if (!fp) {
        return YVEX_ERR_INVALID_ARG;
    }

    p = (const unsigned char *)(text ? text : "");
    fputc('"', fp);
    while (*p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', fp);
            fputc((int)*p, fp);
        } else if (*p == '\n') {
            fputs("\\n", fp);
        } else if (*p == '\r') {
            fputs("\\r", fp);
        } else if (*p == '\t') {
            fputs("\\t", fp);
        } else if (*p < 32u) {
            fprintf(fp, "\\u%04x", (unsigned int)*p);
        } else {
            fputc((int)*p, fp);
        }
        ++p;
    }
    fputc('"', fp);
    return YVEX_OK;
}

static void profile_write_counters(FILE *fp, const yvex_metric_counters *counters)
{
    fprintf(fp, "  \"counters\": {\n");
    fprintf(fp, "    \"prompt_tokens\": %llu,\n", counters->prompt_tokens);
    fprintf(fp, "    \"accepted_tokens\": %llu,\n", counters->accepted_tokens);
    fprintf(fp, "    \"rejected_tokens\": %llu,\n", counters->rejected_tokens);
    fprintf(fp, "    \"chat_turns\": %llu,\n", counters->chat_turns);
    fprintf(fp, "    \"bytes_read\": %llu,\n", counters->bytes_read);
    fprintf(fp, "    \"known_tensor_bytes\": %llu,\n", counters->known_tensor_bytes);
    fprintf(fp, "    \"unsupported_tensor_accounting\": %llu\n",
            counters->unsupported_tensor_accounting);
    fprintf(fp, "  }");
}

int yvex_profile_write_json(const char *path,
                            const yvex_profile_summary *summary,
                            const yvex_metrics *metrics,
                            yvex_error *err)
{
    FILE *fp;
    yvex_metric_counters counters;
    int rc;

    if (!summary || !metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_profile_write_json",
                       "summary and metrics are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = profile_open_output(&fp, path, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_metrics_get_counters(metrics, &counters, err);
    if (rc != YVEX_OK) {
        fclose(fp);
        return rc;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.profile.v1\",\n");
    fprintf(fp, "  \"run_id\": ");
    profile_json_write_string(fp, summary->run_id);
    fprintf(fp, ",\n  \"command\": ");
    profile_json_write_string(fp, summary->command);
    fprintf(fp, ",\n  \"model\": ");
    profile_json_write_string(fp, summary->model_name);
    fprintf(fp, ",\n  \"backend\": ");
    profile_json_write_string(fp, summary->backend_name);
    fprintf(fp, ",\n  \"status\": ");
    profile_json_write_string(fp, summary->status);
    fprintf(fp, ",\n  \"execution_ready\": %s,\n",
            summary->execution_ready ? "true" : "false");
    fprintf(fp, "  \"generation\": \"unsupported\",\n");
    profile_write_counters(fp, &counters);
    fprintf(fp, "\n}\n");

    fclose(fp);
    yvex_error_clear(err);
    return YVEX_OK;
}
