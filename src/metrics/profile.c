/*
 * YVEX - Metrics/profile JSON writers
 *
 * File: src/metrics/profile.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Writes J0 metrics and profile JSON files for implemented runtime shell
 *   paths. The output intentionally omits generated-token and decode metrics.
 */
#include <yvex/profile.h>

#include "metrics_internal.h"

#include <stdio.h>

static int open_output(FILE **out, const char *path, yvex_error *err, const char *where)
{
    if (!out || !path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    *out = fopen(path, "w");
    if (!*out) {
        yvex_error_setf(err, YVEX_ERR_IO, where, "cannot open output file %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static void write_counters(FILE *fp, const yvex_metric_counters *counters)
{
    fprintf(fp, "  \"counters\": {\n");
    fprintf(fp, "    \"prompt_tokens\": %llu,\n", counters->prompt_tokens);
    fprintf(fp, "    \"accepted_tokens\": %llu,\n", counters->accepted_tokens);
    fprintf(fp, "    \"rejected_tokens\": %llu,\n", counters->rejected_tokens);
    fprintf(fp, "    \"chat_turns\": %llu,\n", counters->chat_turns);
    fprintf(fp, "    \"bytes_read\": %llu,\n", counters->bytes_read);
    fprintf(fp, "    \"known_tensor_bytes\": %llu,\n", counters->known_tensor_bytes);
    fprintf(fp, "    \"unsupported_tensor_accounting\": %llu\n", counters->unsupported_tensor_accounting);
    fprintf(fp, "  }");
}

int yvex_metrics_write_json(const char *path,
                            const yvex_metrics *metrics,
                            yvex_error *err)
{
    FILE *fp;
    yvex_metric_counters counters;
    yvex_metric_phase_summary phase;
    int rc;
    unsigned long i;
    int first_phase = 1;

    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_write_json", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = open_output(&fp, path, err, "yvex_metrics_write_json");
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_metrics_get_counters(metrics, &counters, err);
    if (rc != YVEX_OK) {
        fclose(fp);
        return rc;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.metrics.v1\",\n");
    fprintf(fp, "  \"status\": \"accepted-only\",\n");
    write_counters(fp, &counters);
    fprintf(fp, ",\n");
    fprintf(fp, "  \"phases\": [\n");
    for (i = 0; i <= (unsigned long)YVEX_METRIC_PHASE_TOTAL; ++i) {
        rc = yvex_metrics_get_phase(metrics, (yvex_metric_phase)i, &phase, err);
        if (rc != YVEX_OK) {
            fclose(fp);
            return rc;
        }
        if (phase.count == 0) {
            continue;
        }
        if (!first_phase) {
            fprintf(fp, ",\n");
        }
        first_phase = 0;
        fprintf(fp, "    {\"name\": ");
        yvex_json_write_string(fp, phase.name);
        fprintf(fp, ", \"count\": %llu, \"total_ns\": %llu, \"last_ns\": %llu, \"min_ns\": %llu, \"max_ns\": %llu}",
                phase.count, phase.total_ns, phase.last_ns, phase.min_ns, phase.max_ns);
    }
    fprintf(fp, "\n  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    yvex_error_clear(err);
    return YVEX_OK;
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
    rc = open_output(&fp, path, err, "yvex_profile_write_json");
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
    yvex_json_write_string(fp, summary->run_id);
    fprintf(fp, ",\n  \"command\": ");
    yvex_json_write_string(fp, summary->command);
    fprintf(fp, ",\n  \"model\": ");
    yvex_json_write_string(fp, summary->model_name);
    fprintf(fp, ",\n  \"backend\": ");
    yvex_json_write_string(fp, summary->backend_name);
    fprintf(fp, ",\n  \"status\": ");
    yvex_json_write_string(fp, summary->status);
    fprintf(fp, ",\n  \"execution_ready\": %s,\n", summary->execution_ready ? "true" : "false");
    fprintf(fp, "  \"generation\": \"unsupported\",\n");
    write_counters(fp, &counters);
    fprintf(fp, "\n}\n");

    fclose(fp);
    yvex_error_clear(err);
    return YVEX_OK;
}
