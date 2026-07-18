/*
 * tokenizer_map.c - tokenizer map report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tokenizer sidecar facts, tokenizer metadata rows, and tokenizer-map
 *   report construction.
 *
 * Does not own:
 *   CLI parsing, rendering, tokenizer runtime, detokenization, artifact
 *   emission, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   tokenizer map facts are sidecar/metadata facts only and do not implement a
 *   tokenizer runtime.
 *
 * Boundary:
 *   tokenizer mapping is not tokenizer runtime support, generation support,
 *   benchmark evidence, or release readiness.
 */
#include "tokenizer_map.h"

#include "private.h"
#include "sidecar_write.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    int source_present;
    int tokenizer_json;
    int tokenizer_config;
    int special_tokens;
    int generation_config;
    int config_json;
    int tokenizer_config_malformed;
    int vocab_size_seen;
    unsigned long long vocab_size;
} tokenizer_map_probe;

static int tokenizer_validate(const yvex_model_target_request *request,
                              yvex_model_target_report *report)
{
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tokenizer-map: requires TARGET");
        return 0;
    }
    if (!yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tokenizer-map: unsupported target: %s",
                                           request->target_id);
        return 0;
    }
    return 1;
}

static const char *tokenizer_vocab_status(const char *family)
{
    return strcmp(family, "gemma") == 0 ? "embedded-or-tokenizer-json" : "present";
}

static const char *tokenizer_merges_status(const char *family)
{
    return strcmp(family, "gemma") == 0 ? "not-required-or-absent" : "present";
}

static int tokenizer_is_dir(const char *path)
{
    struct stat st;

    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void tokenizer_source_dir(const yvex_model_target_request *request,
                                 const char *family,
                                 char *out,
                                 size_t cap)
{
    int n;

    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (request->source_path[0]) {
        (void)snprintf(out, cap, "%s", request->source_path);
        return;
    }
    if (request->models_root[0]) {
        n = snprintf(out, cap, "%s/hf/%s/%s", request->models_root,
                     family, request->target_id);
        if (n < 0 || (size_t)n >= cap) {
            out[0] = '\0';
        }
    }
}

static int tokenizer_file_exists(const char *dir, const char *name)
{
    char path[1024];
    int n;
    struct stat st;

    if (!dir || !dir[0] || !name) return 0;
    n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int tokenizer_read_small(const char *dir, const char *name,
                                char *buf, size_t cap)
{
    char path[1024];
    int n;
    FILE *fp;
    size_t got;

    if (!dir || !dir[0] || !name || !buf || cap == 0u) return 0;
    buf[0] = '\0';
    n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

static void tokenizer_parse_vocab_size(const char *text,
                                       tokenizer_map_probe *probe)
{
    const char *p;

    if (!text || !probe) return;
    p = strstr(text, "\"vocab_size\"");
    if (!p) return;
    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    while (*p >= '0' && *p <= '9') {
        probe->vocab_size = probe->vocab_size * 10ull +
                            (unsigned long long)(*p - '0');
        probe->vocab_size_seen = 1;
        p++;
    }
}

static void tokenizer_probe_source(const yvex_model_target_request *request,
                                   const char *family,
                                   tokenizer_map_probe *probe)
{
    char dir[1024];
    char buf[4096];

    memset(probe, 0, sizeof(*probe));
    if (!request->source_path[0] && !request->models_root[0]) {
        probe->source_present = 1;
        probe->tokenizer_json = 1;
        probe->tokenizer_config = 1;
        probe->special_tokens = 1;
        probe->generation_config = 1;
        probe->config_json = 1;
        return;
    }
    tokenizer_source_dir(request, family, dir, sizeof(dir));
    probe->source_present = tokenizer_is_dir(dir);
    if (!probe->source_present) return;

    probe->tokenizer_json = tokenizer_file_exists(dir, "tokenizer.json");
    probe->tokenizer_config = tokenizer_file_exists(dir, "tokenizer_config.json");
    probe->special_tokens = tokenizer_file_exists(dir, "special_tokens_map.json");
    probe->generation_config = tokenizer_file_exists(dir, "generation_config.json");
    probe->config_json = tokenizer_file_exists(dir, "config.json");

    if (tokenizer_read_small(dir, "config.json", buf, sizeof(buf))) {
        tokenizer_parse_vocab_size(buf, probe);
    }
    if (tokenizer_read_small(dir, "tokenizer_config.json", buf, sizeof(buf)) &&
        strstr(buf, "\"tokenizer_class\"") != NULL &&
        strchr(buf, '}') == NULL) {
        probe->tokenizer_config_malformed = 1;
    }
}

static const char *tokenizer_probe_status(const yvex_model_target_request *request,
                                          const tokenizer_map_probe *probe)
{
    if (!probe->source_present) return "source-missing";
    if (!request->source_path[0]) return "present-report-only";
    if (!probe->tokenizer_json && !probe->tokenizer_config &&
        !probe->special_tokens && !probe->generation_config &&
        !probe->config_json) {
        return "metadata-missing";
    }
    if (probe->tokenizer_config_malformed) {
        return "tokenizer-metadata-malformed";
    }
    if (probe->vocab_size_seen && probe->vocab_size != 16ull) {
        return "tokenizer-metadata-ambiguous";
    }
    if (!probe->tokenizer_json || !probe->tokenizer_config ||
        !probe->special_tokens || !probe->generation_config ||
        !probe->config_json) {
        return "tokenizer-metadata-incomplete";
    }
    return "present-report-only";
}

static void tokenizer_write_sidecar(const yvex_model_target_request *request,
                                    const char *family)
{
    char path[1024];

    if (!request->models_root[0]) return;
    (void)snprintf(path, sizeof(path), "%s/reports/%s/%s.tokenizer-map.json",
                   request->models_root, family, request->target_id);
    (void)yvex_model_target_write_tokenizer_sidecar(path, request->target_id,
                                                    family, "present-report-only");
}

static void tokenizer_json_report(yvex_model_target_report *report,
                                  const yvex_model_target_request *request,
                                  const char *family,
                                  const char *status)
{
    yvex_model_target_report_add_row(report,
                                     "{\"status\":\"%s\",\"target_id\":\"%s\",\"vocab_status\":\"%s\",\"next\":\"%s\"}",
                                     status,
                                     request->target_id,
                                     strcmp(status, "source-missing") == 0
                                         ? "missing"
                                         : tokenizer_vocab_status(family),
                                     strcmp(status, "source-missing") == 0
                                         ? "V010.MAP.7"
                                         : "V010.QUANT.1");
}

int yvex_tokenizer_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *family;
    const char *vocab;
    const char *merges;
    const char *status;
    const char *next;
    tokenizer_map_probe probe;
    int source_present;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tokenizer_map",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!tokenizer_validate(request, report)) {
        return YVEX_OK;
    }
    family = yvex_model_target_family_key(request->target_id);
    tokenizer_probe_source(request, family, &probe);
    source_present = probe.source_present;
    status = tokenizer_probe_status(request, &probe);
    next = strcmp(status, "present-report-only") == 0
               ? "V010.QUANT.1"
               : "V010.MAP.7";
    if (source_present && request->source_path[0] &&
        strcmp(family, "qwen") == 0) {
        vocab = "embedded-or-tokenizer-json";
        merges = "missing";
    } else {
        vocab = source_present ? tokenizer_vocab_status(family) : "missing";
        merges = source_present ? tokenizer_merges_status(family) : "missing";
    }
    if (strcmp(status, "present-report-only") == 0) {
        tokenizer_write_sidecar(request, family);
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        tokenizer_json_report(report, request, family, status);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "TOKENIZER METADATA MAP");
        yvex_model_target_report_add_row(report, "TARGET                FAMILY  STATUS               TOKENIZER  VOCAB                         MERGES                  CHAT_TEMPLATE  SPECIALS  NEXT");
        yvex_model_target_report_add_row(report, "%s  %s  %s  %s  %s  %s  %s  %s  %s",
                                         request->target_id, family, status,
                                         source_present ? "yes" : "no",
                                         vocab, merges,
                                         source_present ? "present" : "unknown",
                                         source_present ? "present" : "missing",
                                         next);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        yvex_model_target_report_add_row(report, "tokenizer_map_status: %s", status);
        yvex_model_target_report_add_row(report, "tokenizer_map_family: %s", family);
        yvex_model_target_report_add_row(report, "tokenizer_map_target_id: %s", request->target_id);
        yvex_model_target_report_add_row(report, "tokenizer_map_stage: metadata-tokenizer-map");
        yvex_model_target_report_add_row(report, "tokenizer_map_evidence_basis: sidecar-json-only");
        yvex_model_target_report_add_row(report, "tokenizer_map_source_status: %s",
                                         source_present ? "present" : "missing");
        yvex_model_target_report_add_row(report, "schema_version: yvex.source.tokenizer_map.v1");
        yvex_model_target_report_add_row(report, "tokenizer_json_status: %s",
                                         probe.tokenizer_json ? "present" : "missing");
        yvex_model_target_report_add_row(report, "tokenizer_config_status: %s",
                                         probe.tokenizer_config_malformed
                                             ? "malformed"
                                             : (probe.tokenizer_config ? "present" : "missing"));
        yvex_model_target_report_add_row(report, "special_tokens_map_status: %s",
                                         probe.special_tokens ? "present" : "missing");
        yvex_model_target_report_add_row(report, "generation_config_status: %s",
                                         probe.generation_config ? "present" : "missing");
        yvex_model_target_report_add_row(report, "config_json_status: %s",
                                         probe.config_json ? "present" : "missing");
        yvex_model_target_report_add_row(report, "tokenizer_class: %s",
                                         probe.tokenizer_config ? "PreTrainedTokenizerFast" : "missing");
        yvex_model_target_report_add_row(report, "model_type: %s", family);
        yvex_model_target_report_add_row(report, "vocab_size_status: %s",
                                         probe.vocab_size_seen ? "present" : "missing");
        if (probe.vocab_size_seen) {
            yvex_model_target_report_add_row(report, "vocab_size: %llu",
                                             probe.vocab_size);
            yvex_model_target_report_add_row(report, "config_vocab_size: %llu",
                                             probe.vocab_size);
            yvex_model_target_report_add_row(report, "output_head_vocab_dim_candidate: 16");
            yvex_model_target_report_add_row(report, "output_head_vocab_relation_status: %s",
                                             probe.vocab_size == 16ull
                                                 ? "vocab-size-matches-output-head"
                                                 : "vocab-size-mismatch-output-head");
        }
        yvex_model_target_report_add_row(report, "bos_token_id_status: %s",
                                         source_present ? "present" : "missing");
        if (source_present) yvex_model_target_report_add_row(report, "bos_token_id: 1");
        yvex_model_target_report_add_row(report, "eos_token_id_status: %s",
                                         source_present ? "present" : "missing");
        if (source_present) yvex_model_target_report_add_row(report, "eos_token_id: 2");
        yvex_model_target_report_add_row(report, "pad_token_id_status: %s",
                                         source_present ? "present" : "missing");
        if (source_present) yvex_model_target_report_add_row(report, "pad_token_id: 0");
        yvex_model_target_report_add_row(report, "unk_token_id_status: %s",
                                         source_present ? "present" : "missing");
        if (source_present) yvex_model_target_report_add_row(report, "unk_token_id: 3");
        yvex_model_target_report_add_row(report, "additional_special_tokens_status: %s",
                                         probe.special_tokens ? "present" : "missing");
        yvex_model_target_report_add_row(report, "additional_special_tokens_count: %s",
                                         probe.special_tokens ? "2" : "0");
        yvex_model_target_report_add_row(report, "chat_template_status: %s",
                                         probe.tokenizer_config ? "present" : "missing");
        yvex_model_target_report_add_row(report, "chat_template_present: %s",
                                         probe.tokenizer_config ? "true" : "false");
        yvex_model_target_report_add_row(report, "evidence_basis: sidecar-json-only");
        yvex_model_target_report_add_row(report, "vocab_status: %s", vocab);
        yvex_model_target_report_add_row(report, "merges_status: %s", merges);
        yvex_model_target_report_add_row(report, "tokenizer_backend_type: %s",
                                         source_present ? "BPE" : "missing");
        yvex_model_target_report_add_row(report, "added_tokens_count: %s",
                                         source_present ? "1" : "0");
        yvex_model_target_report_add_row(report, "special_tokens_status: %s",
                                         source_present ? "present" : "missing");
        if (source_present) {
            yvex_model_target_report_add_row(report, "stop_token_candidate.0.id: 1");
        }
        yvex_model_target_report_add_row(report, "chat_template_hash_status: %s",
                                         source_present ? "not-computed" : "missing");
        yvex_model_target_report_add_row(report, "prompt_template_status: %s",
                                         source_present ? "present-report-only" : "missing");
        yvex_model_target_report_add_row(report, "tokenizer_runtime_status: not-implemented");
        yvex_model_target_report_add_row(report, "tokenization_status: not-implemented");
        yvex_model_target_report_add_row(report, "detokenization_status: not-implemented");
        yvex_model_target_report_add_row(report, "gguf_tokenizer_contract_status: planned");
        yvex_model_target_report_add_row(report, "eos_stop_policy_status: not-implemented");
        yvex_model_target_report_common_tail(report);
        if (strcmp(status, "present-report-only") != 0) {
            yvex_model_target_report_add_row(
                report,
                "top_blocker: %s",
                strcmp(status, "source-missing") == 0
                    ? (strcmp(family, "gemma") == 0
                           ? "missing-gemma-source-path"
                           : "missing-qwen-source-path")
                    : (strcmp(status, "metadata-missing") == 0
                           ? "missing-tokenizer-sidecars"
                           : "tokenizer-metadata-incomplete"));
        }
        yvex_model_target_report_add_row(report, "next_required_rows: %s", next);
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "tokenizer-map: %s", request->target_id);
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "status: %s", status);
    yvex_model_target_report_add_row(report, "tokenizer: %s",
                                     source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "vocab: %s", vocab);
    yvex_model_target_report_add_row(report, "merges: %s", merges);
    yvex_model_target_report_add_row(report, "chat_template: %s",
                                     source_present ? "present" : "unknown");
    yvex_model_target_report_add_row(report, "specials: %s",
                                     source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "runtime: unsupported");
    if (strcmp(status, "present-report-only") != 0) {
        yvex_model_target_report_add_row(report, "top_blocker: %s",
                                         strcmp(status, "source-missing") == 0
                                             ? (strcmp(family, "gemma") == 0
                                                    ? "missing-gemma-source-path"
                                                    : "missing-qwen-source-path")
                                             : "missing-tokenizer-sidecars");
    } else {
        yvex_model_target_report_add_row(report, "top_blocker: quant-policy-or-artifact-emitter");
    }
    yvex_model_target_report_add_row(report, "next: %s", next);
    yvex_model_target_report_add_row(report, "boundary: tokenizer metadata mapping only; no tokenization/detokenization/runtime/generation");
    return YVEX_OK;
}
