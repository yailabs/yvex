/*
 * yvex_profile.c - Runtime profile document boundary.
 *
 * This file owns the canonical source boundary for profile output and runtime
 * profile documents. It does not create benchmark numbers or generation
 * readiness claims.
 */

#include <yvex/profile.h>
#include "yvex_cli_out.h"

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
            yvex_cli_out_fputs("\\n", fp);
        } else if (*p == '\r') {
            yvex_cli_out_fputs("\\r", fp);
        } else if (*p == '\t') {
            yvex_cli_out_fputs("\\t", fp);
        } else if (*p < 32u) {
            yvex_cli_out_writef(fp, "\\u%04x", (unsigned int)*p);
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
    yvex_cli_out_writef(fp, "  \"counters\": {\n");
    yvex_cli_out_writef(fp, "    \"prompt_tokens\": %llu,\n", counters->prompt_tokens);
    yvex_cli_out_writef(fp, "    \"accepted_tokens\": %llu,\n", counters->accepted_tokens);
    yvex_cli_out_writef(fp, "    \"rejected_tokens\": %llu,\n", counters->rejected_tokens);
    yvex_cli_out_writef(fp, "    \"chat_turns\": %llu,\n", counters->chat_turns);
    yvex_cli_out_writef(fp, "    \"bytes_read\": %llu,\n", counters->bytes_read);
    yvex_cli_out_writef(fp, "    \"known_tensor_bytes\": %llu,\n", counters->known_tensor_bytes);
    yvex_cli_out_writef(fp, "    \"unsupported_tensor_accounting\": %llu\n",
            counters->unsupported_tensor_accounting);
    yvex_cli_out_writef(fp, "  }");
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

    yvex_cli_out_writef(fp, "{\n");
    yvex_cli_out_writef(fp, "  \"schema\": \"yvex.profile.v1\",\n");
    yvex_cli_out_writef(fp, "  \"run_id\": ");
    profile_json_write_string(fp, summary->run_id);
    yvex_cli_out_writef(fp, ",\n  \"command\": ");
    profile_json_write_string(fp, summary->command);
    yvex_cli_out_writef(fp, ",\n  \"model\": ");
    profile_json_write_string(fp, summary->model_name);
    yvex_cli_out_writef(fp, ",\n  \"backend\": ");
    profile_json_write_string(fp, summary->backend_name);
    yvex_cli_out_writef(fp, ",\n  \"status\": ");
    profile_json_write_string(fp, summary->status);
    yvex_cli_out_writef(fp, ",\n  \"execution_ready\": %s,\n",
            summary->execution_ready ? "true" : "false");
    yvex_cli_out_writef(fp, "  \"generation\": \"unsupported\",\n");
    profile_write_counters(fp, &counters);
    yvex_cli_out_writef(fp, "\n}\n");

    fclose(fp);
    yvex_error_clear(err);
    return YVEX_OK;
}
