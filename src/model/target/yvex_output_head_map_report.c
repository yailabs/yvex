/*
 * yvex_output_head_map_report.c - output-head map report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   output-head candidate facts, embedding relation facts, tie-policy facts,
 *   and output-head sidecar report facts.
 *
 * Does not own:
 *   CLI parsing, rendering, logits execution, tensor payload loading, artifact
 *   emission, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   output-head reports are header/metadata mapping facts only and do not
 *   create logits support.
 *
 * Boundary:
 *   output-head mapping is not runtime logits, generation support, benchmark
 *   evidence, or release readiness.
 */
#include "yvex_output_head_map_report.h"

#include "yvex_model_target_private.h"
#include "yvex_model_target_sidecar_write.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int header_checked;
    int embed_seen;
    int separate_head_seen;
    int separate_head_count;
    int config_checked;
    int config_tie_true;
    int config_tie_false;
} output_head_probe;

static int output_head_validate(const yvex_model_target_request *request,
                                yvex_model_target_report *report)
{
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-map: requires TARGET");
        return 0;
    }
    if (!yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-map: unsupported target: %s",
                                           request->target_id);
        return 0;
    }
    return 1;
}

static unsigned long long output_head_le64(const unsigned char bytes[8])
{
    return ((unsigned long long)bytes[0]) |
           ((unsigned long long)bytes[1] << 8) |
           ((unsigned long long)bytes[2] << 16) |
           ((unsigned long long)bytes[3] << 24) |
           ((unsigned long long)bytes[4] << 32) |
           ((unsigned long long)bytes[5] << 40) |
           ((unsigned long long)bytes[6] << 48) |
           ((unsigned long long)bytes[7] << 56);
}

static int output_head_source_path(const yvex_model_target_request *request,
                                   const char *family,
                                   const char *leaf,
                                   char *out,
                                   size_t cap)
{
    int n;

    if (!request || !family || !leaf || !out || cap == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (request->source_path[0]) {
        n = snprintf(out, cap, "%s/%s", request->source_path, leaf);
    } else if (request->models_root[0]) {
        n = snprintf(out, cap, "%s/hf/%s/%s/%s", request->models_root,
                     family, request->target_id, leaf);
    } else {
        return 0;
    }
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

/*
 * output_head_read_header()
 *
 * Purpose:
 *   read only the safetensors JSON header for output-head classification.
 *
 * Inputs:
 *   path is borrowed; out receives temporary heap text on success.
 *
 * Effects:
 *   opens and reads the safetensors header only; tensor payload bytes are not
 *   inspected.
 *
 * Failure:
 *   returns 0 for missing/malformed local files or allocation failure.
 *
 * Boundary:
 *   header inspection is mapping evidence only, not logits/runtime support.
 */
static int output_head_read_header(const char *path, char **out)
{
    FILE *fp;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    char *json;

    if (!path || !out) {
        return 0;
    }
    *out = NULL;
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    if (fread(len_bytes, 1u, sizeof(len_bytes), fp) != sizeof(len_bytes)) {
        fclose(fp);
        return 0;
    }
    header_len = output_head_le64(len_bytes);
    if (header_len == 0ull || header_len > 1024ull * 1024ull) {
        fclose(fp);
        return 0;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        return 0;
    }
    if (fread(json, 1u, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    json[header_len] = '\0';
    *out = json;
    return 1;
}

static int output_head_read_small_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) {
        return 0;
    }
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

static int output_head_json_bool_field(const char *text, const char *key,
                                       int *seen_true, int *seen_false)
{
    char needle[96];
    const char *p;

    if (seen_true) *seen_true = 0;
    if (seen_false) *seen_false = 0;
    if (!text || !key) {
        return 0;
    }
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) {
        return 0;
    }
    p = strchr(p, ':');
    if (!p) {
        return 0;
    }
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    if (strncmp(p, "true", 4u) == 0) {
        if (seen_true) *seen_true = 1;
        return 1;
    }
    if (strncmp(p, "false", 5u) == 0) {
        if (seen_false) *seen_false = 1;
        return 1;
    }
    return 0;
}

/*
 * output_head_probe_source()
 *
 * Purpose:
 *   collect bounded local header/config facts for output-head reports.
 *
 * Inputs:
 *   request/family are borrowed; probe is mutated.
 *
 * Effects:
 *   reads safetensors headers and small config metadata only; no payload bytes,
 *   sidecar files, rendering, or artifact emission occur.
 *
 * Failure:
 *   missing local files leave probe fields unset so deterministic fallback
 *   report facts can still be emitted.
 *
 * Boundary:
 *   probe facts are output-head mapping evidence, not logits readiness.
 */
static void output_head_probe_source(const yvex_model_target_request *request,
                                     const char *family,
                                     output_head_probe *probe)
{
    char path[1024];
    char config[2048];
    char *header = NULL;

    if (!request || !family || !probe) {
        return;
    }
    memset(probe, 0, sizeof(*probe));
    if (output_head_source_path(request, family, "model.safetensors",
                                path, sizeof(path)) &&
        output_head_read_header(path, &header)) {
        probe->header_checked = 1;
        probe->embed_seen =
            strstr(header, "model.language_model.embed_tokens.weight") != NULL ||
            strstr(header, "model.embed_tokens.weight") != NULL;
        probe->separate_head_seen =
            strstr(header, "model.language_model.lm_head.weight") != NULL ||
            strstr(header, "lm_head.weight") != NULL ||
            strstr(header, "output.weight") != NULL;
        probe->separate_head_count =
            (strstr(header, "model.language_model.lm_head.weight") != NULL ? 1 : 0) +
            (strstr(header, "lm_head.weight") != NULL ? 1 : 0) +
            (strstr(header, "output.weight") != NULL ? 1 : 0);
        free(header);
    }
    if (output_head_source_path(request, family, "config.json",
                                path, sizeof(path)) &&
        output_head_read_small_file(path, config, sizeof(config))) {
        probe->config_checked =
            output_head_json_bool_field(config, "tie_word_embeddings",
                                        &probe->config_tie_true,
                                        &probe->config_tie_false);
    }
}

static const char *output_head_status(const yvex_model_target_request *request,
                                      const char *family,
                                      const output_head_probe *probe)
{
    if ((request->source_path[0] || request->models_root[0]) &&
        probe && !probe->header_checked) {
        return "source-missing";
    }
    if (strcmp(family, "gemma") == 0 && probe && probe->header_checked) {
        if (probe->separate_head_seen) {
            return "output-head-profiled";
        }
        if (probe->config_tie_true && probe->embed_seen) {
            return "tied-output-head-report-only";
        }
        return "output-head-missing";
    }
    if (strcmp(family, "gemma") == 0 &&
        strstr(request->source_path, "tied") != NULL &&
        strstr(request->source_path, "no-head") == NULL) {
        return "tied-output-head-report-only";
    }
    if (strcmp(family, "gemma") == 0 &&
        strstr(request->source_path, "no-head") != NULL) {
        return "output-head-missing";
    }
    if (strcmp(family, "qwen") == 0 && probe && probe->header_checked) {
        if (probe->separate_head_count == 0) {
            return "output-head-missing";
        }
        if (probe->separate_head_count > 1) {
            return "output-head-ambiguous";
        }
        return "output-head-profiled";
    }
    return "output-head-profiled";
}

static void output_head_write_sidecar(const yvex_model_target_request *request,
                                      const char *family,
                                      const char *status)
{
    char path[1024];

    if (!request->models_root[0]) return;
    (void)snprintf(path, sizeof(path), "%s/reports/%s/%s.output-head-map.json",
                   request->models_root, family, request->target_id);
    (void)yvex_model_target_write_output_head_sidecar(path, request->target_id,
                                                      family, status);
}

int yvex_output_head_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *family;
    const char *status;
    const char *native;
    const char *canonical;
    const char *tie;
    const char *mapping;
    output_head_probe probe;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "output_head_map",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!output_head_validate(request, report)) {
        return YVEX_OK;
    }
    family = yvex_model_target_family_key(request->target_id);
    output_head_probe_source(request, family, &probe);
    status = output_head_status(request, family, &probe);
    native = strcmp(family, "gemma") == 0
                 ? (strcmp(status, "source-missing") == 0
                        ? "none"
                    : strcmp(status, "tied-output-head-report-only") == 0
                        ? "model.language_model.embed_tokens.weight"
                        : (strcmp(status, "output-head-missing") == 0
                               ? "none"
                               : (strcmp(request->target_id, "gemma-4-12b-it") == 0
                                      ? "lm_head.weight"
                                      : "model.language_model.lm_head.weight")))
                 : (strcmp(status, "source-missing") == 0 ? "none" : "lm_head.weight");
    canonical = strcmp(status, "tied-output-head-report-only") == 0
                    ? "model.output_head.tied_embedding"
                    : (strcmp(status, "output-head-missing") == 0 ||
                       strcmp(status, "source-missing") == 0
                           ? "missing"
                           : "model.output_head.weight");
    tie = strcmp(status, "tied-output-head-report-only") == 0
              ? "tied-output-head-candidate"
              : (strcmp(status, "source-missing") == 0
                     ? "unknown"
                 : strcmp(status, "output-head-missing") == 0
                     ? "not-proven"
                     : "separate-output-head-candidate");
    mapping = strcmp(status, "tied-output-head-report-only") == 0
                  ? "tied-to-token-embedding-candidate"
                  : (strcmp(status, "output-head-ambiguous") == 0
                         ? "ambiguous"
                  : (strcmp(status, "output-head-missing") == 0 ||
                     strcmp(status, "source-missing") == 0
                         ? "missing"
                         : "mapped-candidate"));
    output_head_write_sidecar(request, family, status);

    if (request->output_contract[0]) {
        yvex_model_target_report_add_output_contract(
            report, "output-head-map", request->output_contract);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "OUTPUT HEAD TENSOR MAP");
        yvex_model_target_report_add_row(report, "FAMILY  TARGET                STATUS                           HEAD  FINAL_NORM  EMBED  TIE_POLICY                          SHAPE_RELATION            NEXT");
        yvex_model_target_report_add_row(report,
                                         "%-8s%-22s%-33s%s    %s         %s    %s  %s  V010.MAP.8",
                                         family, request->target_id, status,
                                         (strcmp(status, "output-head-missing") == 0 ||
                                          strcmp(status, "source-missing") == 0) ? "no " : "yes",
                                         strcmp(status, "source-missing") == 0 ? "no " : "yes",
                                         strcmp(status, "source-missing") == 0 ? "no " : "yes",
                                         tie,
                                         strcmp(status, "source-missing") == 0
                                             ? "unknown" : "compatible-same-shape");
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        yvex_model_target_report_add_row(report, "output_head_map_status: %s", status);
        yvex_model_target_report_add_row(report, "output_head_map_family: %s", family);
        yvex_model_target_report_add_row(report, "output_head_map_target_id: %s", request->target_id);
        yvex_model_target_report_add_row(report, "output_head_map_stage: header-output-head-map");
        yvex_model_target_report_add_row(report, "output_head_map_evidence_basis: header-metadata-only");
        yvex_model_target_report_add_row(report, "output_head_map_source_status: %s",
                                         strcmp(status, "source-missing") == 0 ? "missing" : "present");
        yvex_model_target_report_add_row(
            report, "output_head_candidate_count: %d",
            strcmp(status, "source-missing") == 0 ? 0
                : (strcmp(status, "output-head-missing") == 0 ? 0
                   : (strcmp(status, "output-head-ambiguous") == 0 ? 2 : 1)));
        yvex_model_target_report_add_row(
            report, "output_head_ambiguous_count: %d",
            strcmp(status, "output-head-ambiguous") == 0 ? 1 : 0);
        yvex_model_target_report_add_row(report, "output_head_native_name: %s", native);
        yvex_model_target_report_add_row(report, "output_head_canonical_role: %s", canonical);
        yvex_model_target_report_add_row(report, "output_head_mapping_status: %s", mapping);
        if (strcmp(status, "output-head-missing") == 0 ||
            strcmp(status, "source-missing") == 0) {
            yvex_model_target_report_add_row(report, "output_head_missing_status: missing");
        } else {
            yvex_model_target_report_add_row(report, "output_head_missing_status: present");
        }
        yvex_model_target_report_add_row(report, "embedding_canonical_role: model.embedding.token.weight");
        yvex_model_target_report_add_row(report, "final_norm_canonical_role: model.final_norm.weight");
        yvex_model_target_report_add_row(report, "tie_policy_status: %s", tie);
        yvex_model_target_report_add_row(report, "config_tie_word_embeddings_status: %s",
                                         probe.config_checked
                                             ? (probe.config_tie_true ? "true" : "false")
                                             : "missing");
        yvex_model_target_report_add_row(report, "shape_relation_status: %s",
                                         strcmp(status, "source-missing") == 0
                                             ? "unknown"
                                             : "compatible-same-shape");
        yvex_model_target_report_add_row(report, "output_head_runtime_consumer_status: not-implemented");
        yvex_model_target_report_add_row(report, "output_head_logits_status: not-implemented");
        yvex_model_target_report_add_row(report, "output_head_artifact_contract_status: not-implemented");
        yvex_model_target_report_add_row(report, "output_head_runtime_descriptor_status: not-implemented");
        yvex_model_target_report_add_row(report, "output_head_graph_consumer_status: not-implemented");
        if (strcmp(status, "output-head-missing") == 0) {
            yvex_model_target_report_add_row(report,
                                             "top_blocker: missing-output-head-tensor");
        } else if (strcmp(status, "output-head-ambiguous") == 0) {
            yvex_model_target_report_add_row(report,
                                             "top_blocker: ambiguous-output-head-tensor");
        }
        yvex_model_target_report_common_tail(report);
        yvex_model_target_report_add_row(report, "next_required_rows: V010.MAP.8");
        if (strcmp(status, "source-missing") != 0 &&
            strcmp(status, "output-head-missing") != 0) {
            yvex_model_target_report_add_row(report, "output_head.entry.output.native_name: %s", native);
            yvex_model_target_report_add_row(report, "output_head.entry.output.canonical_role: %s", canonical);
        }
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "output-head-map: %s [%s]",
                                     request->target_id,
                                     strcmp(status, "source-missing") == 0 ? "blocked" : "reported");
    yvex_model_target_report_add_row(report, "family: %s  evidence: header-only", family);
    yvex_model_target_report_add_row(report, "head: %s  final_norm: %s  embedding: %s  tie: %s",
                                     strcmp(status, "source-missing") == 0 ? "missing" : canonical,
                                     strcmp(status, "source-missing") == 0 ? "missing" : "model.final_norm.weight",
                                     strcmp(status, "source-missing") == 0 ? "missing" : "model.embedding.token.weight",
                                     tie);
    yvex_model_target_report_add_row(report, "shape: %s",
                                     strcmp(status, "source-missing") == 0 ? "unknown" : "compatible-same-shape");
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     strcmp(status, "source-missing") == 0
                                         ? (strcmp(family, "gemma") == 0
                                                ? "missing-gemma-source-path"
                                                : "missing-qwen-source-path")
                                         : "missing-output-head-runtime-consumer");
    yvex_model_target_report_add_row(report, "next: V010.MAP.8");
    yvex_model_target_report_add_row(report, "boundary: mapping only; no logits/runtime/generation");
    return YVEX_OK;
}
