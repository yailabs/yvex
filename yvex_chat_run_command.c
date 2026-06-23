/*
 * YVEX - Run command formatters
 *
 * File: yvex_chat_run_command.c
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Formats diagnostic runtime accepted-only run results in plain text or JSON. The result
 *   envelope states that generation is unsupported.
 *
 * Implements:
 *   - yvex_run_command_plain
 *   - yvex_run_command_json
 *
 * Invariants:
 *   - no generated assistant text is emitted
 *   - JSON output is deterministic and has no trailing commas
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_chat_runtime
 */
#include "yvex_chat_internal.h"

#include <string.h>

static void json_print_escaped(FILE *fp, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

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
}

int yvex_run_command_plain(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) {
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "run status: accepted-only\n");
    fprintf(fp, "model: %s\n", result->model_name);
    fprintf(fp, "backend: %s\n", result->backend_name);
    fprintf(fp, "session_state: %s\n", result->session_state);
    fprintf(fp, "prompt_tokens: %llu\n", result->prompt_tokens);
    fprintf(fp, "accepted_tokens: %llu\n", result->accepted_tokens);
    fprintf(fp, "position: %llu\n", result->position);
    fprintf(fp, "execution_ready: false\n");
    fprintf(fp, "generation: %s\n", result->generation);
    fprintf(fp, "reason: %s\n", result->reason);
    if (result->run_id[0] != '\0') {
        fprintf(fp, "run_id: %s\n", result->run_id);
    }
    if (result->run_dir[0] != '\0') {
        fprintf(fp, "run_dir: %s\n", result->run_dir);
    }
    if (result->metrics_out[0] != '\0') {
        fprintf(fp, "metrics_out: %s\n", result->metrics_out);
    }
    if (result->trace_out[0] != '\0') {
        fprintf(fp, "trace_out: %s\n", result->trace_out);
    }
    if (result->profile_out[0] != '\0') {
        fprintf(fp, "profile_out: %s\n", result->profile_out);
    }
    return YVEX_OK;
}

int yvex_run_command_json(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) {
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.cli.result.v1\",\n");
    fprintf(fp, "  \"command\": \"run\",\n");
    fprintf(fp, "  \"status\": \"accepted-only\",\n");
    fprintf(fp, "  \"data\": {\n");
    fprintf(fp, "    \"model\": ");
    json_print_escaped(fp, result->model_name);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"backend\": ");
    json_print_escaped(fp, result->backend_name);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"session_state\": ");
    json_print_escaped(fp, result->session_state);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    fprintf(fp, "    \"accepted_tokens\": %llu,\n", result->accepted_tokens);
    fprintf(fp, "    \"position\": %llu,\n", result->position);
    fprintf(fp, "    \"execution_ready\": false,\n");
    fprintf(fp, "    \"generation\": ");
    json_print_escaped(fp, result->generation);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"reason\": ");
    json_print_escaped(fp, result->reason);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"metrics\": {\n");
    fprintf(fp, "      \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    fprintf(fp, "      \"accepted_tokens\": %llu\n", result->accepted_tokens);
    fprintf(fp, "    }");
    if (result->metrics_out[0] != '\0' || result->trace_out[0] != '\0' ||
        result->profile_out[0] != '\0' || result->run_dir[0] != '\0') {
        fprintf(fp, ",\n");
        fprintf(fp, "    \"artifacts\": {\n");
        fprintf(fp, "      \"run_id\": ");
        json_print_escaped(fp, result->run_id);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"run_dir\": ");
        json_print_escaped(fp, result->run_dir);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"metrics_out\": ");
        json_print_escaped(fp, result->metrics_out);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"trace_out\": ");
        json_print_escaped(fp, result->trace_out);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"profile_out\": ");
        json_print_escaped(fp, result->profile_out);
        fprintf(fp, "\n");
        fprintf(fp, "    }");
    }
    fprintf(fp, "\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"error\": null\n");
    fprintf(fp, "}\n");
    return YVEX_OK;
}
