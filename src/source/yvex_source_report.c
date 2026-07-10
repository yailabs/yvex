/*
 * yvex_source_report.c - source pressure report builder.
 *
 * Owner: src/source.
 * Owns: DeepSeek exact-source verification reports and Qwen/Gemma source
 * pressure report facts from local metadata and headers.
 * Does not own: CLI parsing, help, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: report building is header-only and never loads tensor payload bytes.
 * Boundary: source report facts are not artifact emission or runtime readiness.
 */
#include "yvex_source_report.h"
#include "yvex_source_private.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define YVEX_SOURCE_MANIFEST_PROBE_CAP 8192u

static const char *qwen_source_tail_blockers[] = {
    "missing-qwen-tensor-role-map",
    "missing-qwen-tensor-map",
    "missing-qwen-yvex-artifact",
    "missing-qwen-artifact-identity",
    "missing-metal-hardware-profile",
    "missing-metal-backend-feasibility",
    "missing-unified-memory-residency-plan",
    "missing-real-prefill",
    "missing-real-kv-path",
    "missing-real-decode",
    "missing-real-output-head-logits",
    "missing-real-vocabulary-sampling",
    "missing-generation-loop-over-real-state",
    "missing-eval-path",
    "missing-benchmark-path",
};

static const char *gemma_source_tail_blockers[] = {
    "missing-gemma-tensor-role-map",
    "missing-gemma-tensor-map",
    "missing-gemma-yvex-artifact",
    "missing-gemma-artifact-identity",
    "missing-gemma-real-prefill",
    "missing-gemma-real-kv-path",
    "missing-gemma-real-decode",
    "missing-gemma-real-output-head-logits",
    "missing-gemma-real-vocabulary-sampling",
    "missing-gemma-generation-loop-over-real-state",
    "missing-gemma-eval-path",
    "missing-gemma-benchmark-path",
};

static const char *deepseek_source_tail_blockers[] = {
    "missing-deepseek-architecture-ir",
    "reopened-gguf-qtype-abi",
    "missing-deepseek-tensor-role-map",
    "missing-complete-deepseek-gguf",
    "unsupported-deepseek-runtime",
    "unsupported-deepseek-generation",
};

static const yvex_source_family_profile source_family_profiles[] = {
    {
        yvex_deepseek_v4_family_key,
        yvex_deepseek_v4_family_display,
        "deepseek-source-verification",
        yvex_deepseek_v4_target_id,
        "release-source-target",
        yvex_deepseek_v4_model_name,
        "selected-unverified",
        "registered",
        "official-safetensors",
        "safetensors+structured-sidecars",
        "official",
        "upstream-official",
        "safetensors",
        "complete-YVEX-GGUF-not-produced",
        "not-produced",
        "true",
        "false",
        "not-produced",
        "exact-v0.1.0-release-source",
        "raw-config-facts-only",
        "dgx-spark",
        "cuda-release-lane-unsupported",
        "verification-required",
        "missing-source-path",
        "missing-source-manifest",
        "invalid-safetensors-header",
        "missing-source-config",
        "missing-tokenizer-json",
        "missing-deepseek-architecture-ir",
        "V010.GGUF.QTYPE.ABI.1",
        deepseek_source_tail_blockers,
        sizeof(deepseek_source_tail_blockers) /
            sizeof(deepseek_source_tail_blockers[0]),
    },
    {
        "qwen",
        "Qwen",
        "qwen-source-pressure",
        "qwen3-8b",
        "source-model-candidate",
        "Qwen3-8B",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "safetensors+config-tokenizer-sidecars",
        "official",
        "upstream-official",
        "safetensors",
        "future-YVEX-produced-GGUF",
        "planned",
        "true",
        "false",
        "planned",
        "backend-neutral-qwen-source-model-target",
        "causal-decoder-candidate-pending-config",
        "backend-selection-deferred",
        "metal-planned",
        "pending source/config verification",
        "missing-qwen-source-path",
        "missing-qwen-source-manifest",
        "missing-qwen-native-inventory",
        "missing-qwen-source-config",
        "missing-qwen-tokenizer-files",
        "missing-qwen-tensor-role-map",
        "V010.MAP.8",
        qwen_source_tail_blockers,
        sizeof(qwen_source_tail_blockers) / sizeof(qwen_source_tail_blockers[0]),
    },
    {
        "gemma",
        "Gemma",
        "gemma-source-pressure",
        "gemma-4-12b-it",
        "source-model-candidate",
        "Gemma-4-12B-it",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "safetensors+config-tokenizer-sidecars",
        "official",
        "upstream-official",
        "safetensors",
        "future-YVEX-produced-GGUF",
        "planned",
        "true",
        "false",
        "planned",
        "backend-neutral-gemma-source-model-target",
        "dense-candidate-pending-source-config",
        "backend-selection-deferred",
        "cpu-cuda-baseline-planned",
        "pending source/config verification",
        "missing-gemma-source-path",
        "missing-gemma-source-manifest",
        "missing-gemma-native-inventory",
        "missing-gemma-source-config",
        "missing-gemma-tokenizer-files",
        "missing-gemma-tensor-role-map",
        "V010.MAP.8",
        gemma_source_tail_blockers,
        sizeof(gemma_source_tail_blockers) / sizeof(gemma_source_tail_blockers[0]),
    },
};

static const unsigned long source_family_profile_count =
    sizeof(source_family_profiles) / sizeof(source_family_profiles[0]);


const yvex_source_family_profile *yvex_source_report_find_profile(const char *family)
{
    unsigned long i;

    if (!family) {
        return NULL;
    }
    for (i = 0; i < source_family_profile_count; ++i) {
        if (strcmp(source_family_profiles[i].family_key, family) == 0) {
            return &source_family_profiles[i];
        }
    }
    return NULL;
}

int yvex_source_report_target_is_supported(const yvex_source_family_profile *profile,
                                           const char *target)
{
    if (!profile || !target) {
        return 0;
    }
    if (strcmp(profile->family_key, "deepseek") == 0) {
        return yvex_model_target_is_release_target(target);
    }
    if ((strcmp(profile->family_key, "qwen") == 0 &&
         strncmp(target, "qwen", 4) == 0) ||
        (strcmp(profile->family_key, "gemma") == 0 &&
         strncmp(target, "gemma", 5) == 0)) {
        return 1;
    }
    if (strcmp(profile->family_key, "qwen") == 0) {
        return strcmp(target, "qwen3-8b") == 0 ||
               strcmp(target, "qwen-small") == 0 ||
               strcmp(target, "qwen-medium") == 0;
    }
    return strcmp(target, profile->target_id) == 0;
}

static int qwen_source_path_format(char *out, size_t cap, const char *fmt,
                                   const char *a, const char *b)
{
    int n;

    if (!out || cap == 0 || !fmt) {
        return 0;
    }
    if (b) {
        n = snprintf(out, cap, fmt, a ? a : "", b);
    } else {
        n = snprintf(out, cap, fmt, a ? a : "");
    }
    if (n < 0 || (size_t)n >= cap) {
        out[cap - 1] = '\0';
        return 0;
    }
    return 1;
}

static const char *qwen_source_path_basename(const char *path)
{
    const char *slash;

    if (!path || !path[0]) return NULL;
    slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static int qwen_source_target_matches_family_name(const char *family,
                                                  const char *target)
{
    if (!family || !target) return 0;
    if (strcmp(family, "qwen") == 0) {
        return strncmp(target, "qwen", 4) == 0;
    }
    if (strcmp(family, "gemma") == 0) {
        return strncmp(target, "gemma", 5) == 0;
    }
    return 0;
}

static int qwen_source_ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) {
        return 0;
    }
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return 0;
    }
    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int qwen_source_read_small_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) return 0;
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) return 0;
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

static int qwen_source_json_string_field(const char *text,
                                         const char *key,
                                         char *out,
                                         size_t cap)
{
    char needle[96];
    const char *p;
    const char *q;
    size_t len;

    if (out && cap > 0u) out[0] = '\0';
    if (!text || !key || !out || cap == 0u) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    q = strchr(p, '"');
    if (!q) return 0;
    len = (size_t)(q - p);
    if (len >= cap) len = cap - 1u;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

static int qwen_source_json_u64_field(const char *text,
                                      const char *key,
                                      unsigned long long *out)
{
    char needle[96];
    const char *p;
    unsigned long long value = 0;
    int seen = 0;

    if (out) *out = 0;
    if (!text || !key || !out) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    while (*p && isdigit((unsigned char)*p)) {
        value = value * 10ull + (unsigned long long)(*p - '0');
        seen = 1;
        p++;
    }
    if (!seen) return 0;
    *out = value;
    return 1;
}

static const char *qwen_source_repo_basename(const char *repo)
{
    const char *slash = repo ? strrchr(repo, '/') : NULL;
    return slash && slash[1] ? slash + 1 : repo;
}

static void qwen_source_copy_model_display(char *out,
                                           size_t cap,
                                           const char *family,
                                           const char *model_name)
{
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (!model_name || !model_name[0]) return;
    snprintf(out, cap, "%s", model_name);
    if (family && strcmp(family, "gemma") == 0 &&
        strncmp(out, "gemma-", 6) == 0) {
        out[0] = 'G';
    }
}

static int qwen_source_probe_download_identity_file(
    const char *path,
    const char *target,
    const char *family,
    yvex_source_report *report)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    char parsed_target[128];
    char parsed_family[32];
    char repo_id[256];
    char revision[128];
    char source_dir[YVEX_PATH_CAP];
    const char *model_name;

    if (!path || !path[0] || !target || !family || !report) return 0;
    if (access(path, F_OK) != 0) return 0;
    if (!qwen_source_read_small_file(path, buf, sizeof(buf))) return 0;

    memset(parsed_target, 0, sizeof(parsed_target));
    memset(parsed_family, 0, sizeof(parsed_family));
    memset(repo_id, 0, sizeof(repo_id));
    memset(revision, 0, sizeof(revision));
    memset(source_dir, 0, sizeof(source_dir));
    qwen_source_json_string_field(buf, "target_id", parsed_target,
                                  sizeof(parsed_target));
    if (parsed_target[0] && strcmp(parsed_target, target) != 0) return 0;
    qwen_source_json_string_field(buf, "family", parsed_family,
                                  sizeof(parsed_family));
    if (parsed_family[0] && strcmp(parsed_family, family) != 0) return 0;
    qwen_source_json_string_field(buf, "repo_id", repo_id, sizeof(repo_id));
    if (!repo_id[0]) {
        qwen_source_json_string_field(buf, "repo", repo_id, sizeof(repo_id));
    }
    qwen_source_json_string_field(buf, "revision", revision, sizeof(revision));
    qwen_source_json_string_field(buf, "local_source_dir", source_dir,
                                  sizeof(source_dir));
    if (!source_dir[0]) {
        qwen_source_json_string_field(buf, "path", source_dir, sizeof(source_dir));
    }

    snprintf(report->identity_target_id, sizeof(report->identity_target_id), "%s",
             parsed_target[0] ? parsed_target : target);
    snprintf(report->identity_family, sizeof(report->identity_family), "%s",
             parsed_family[0] ? parsed_family : family);
    snprintf(report->identity_repo_id, sizeof(report->identity_repo_id), "%s",
             repo_id[0] ? repo_id : "unknown");
    snprintf(report->identity_revision, sizeof(report->identity_revision), "%s",
             revision[0] ? revision : "main");
    if (source_dir[0]) {
        snprintf(report->identity_local_source_dir,
                 sizeof(report->identity_local_source_dir), "%s", source_dir);
    }
    model_name = qwen_source_repo_basename(repo_id);
    if (model_name && model_name[0]) {
        qwen_source_copy_model_display(report->identity_model,
                                       sizeof(report->identity_model),
                                       family,
                                       model_name);
    }
    report->source_identity_from_download_sidecar = 1;
    return 1;
}

static void qwen_source_probe_map_sidecars(yvex_source_report *report)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    char status[64];
    char coverage[64];
    unsigned long long unmapped = 0;

    if (!report) return;
    if (report->tensor_map_exists &&
        qwen_source_read_small_file(report->tensor_map_path, buf, sizeof(buf))) {
        if (qwen_source_json_string_field(buf, "required_role_coverage_status",
                                          coverage, sizeof(coverage))) {
            if (strcmp(coverage, "required-groups-present") != 0) {
                report->tensor_map_incomplete = 1;
            }
        } else if (qwen_source_json_u64_field(buf, "unmapped_unknown_count",
                                              &unmapped) &&
                   unmapped > 0ull) {
            report->tensor_map_incomplete = 1;
        }
    }
    if (report->output_head_map_exists &&
        qwen_source_read_small_file(report->output_head_map_path, buf, sizeof(buf)) &&
        qwen_source_json_string_field(buf, "output_head_status", status,
                                      sizeof(status)) &&
        strcmp(status, "present") != 0) {
        report->output_head_map_missing = 1;
    }
}

static int qwen_source_stat_kind(const char *path, int want_dir)
{
    struct stat st;

    if (!path || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return want_dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}

static int qwen_source_manifest_file_exists(char *out,
                                            size_t cap,
                                            const char *dir,
                                            const char *name)
{
    char candidate[YVEX_PATH_CAP];

    if (!out || cap == 0 || !dir || dir[0] == '\0' || !name) {
        return 0;
    }
    if (!qwen_source_path_format(candidate, sizeof(candidate), "%s/%s", dir, name)) {
        return 0;
    }
    if (!qwen_source_stat_kind(candidate, 0)) {
        return 0;
    }
    snprintf(out, cap, "%s", candidate);
    return 1;
}

static int qwen_source_check_file(const char *dir, const char *name)
{
    char path[YVEX_PATH_CAP];

    if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, name)) {
        return 0;
    }
    return qwen_source_stat_kind(path, 0);
}

static int qwen_source_is_config_file(const char *name)
{
    return name && (strcmp(name, "config.json") == 0 ||
                    strcmp(name, "generation_config.json") == 0);
}

static int qwen_source_is_tokenizer_file(const char *name)
{
    return name && (strcmp(name, "tokenizer.json") == 0 ||
                    strcmp(name, "tokenizer_config.json") == 0);
}

static int qwen_source_is_sidecar_file(const char *name)
{
    if (!name) {
        return 0;
    }
    return qwen_source_is_config_file(name) ||
           qwen_source_is_tokenizer_file(name) ||
           strcmp(name, "README.md") == 0 ||
           qwen_source_ends_with(name, ".json");
}

static unsigned long long qwen_source_stat_size_bytes(const struct stat *st)
{
    if (!st || st->st_size <= 0) {
        return 0;
    }
    return (unsigned long long)st->st_size;
}

static void qwen_source_scan_top_footprint(const char *dir,
                                           yvex_source_report *report)
{
    DIR *dp;
    struct dirent *ent;

    if (!dir || dir[0] == '\0' || !report) {
        return;
    }
    dp = opendir(dir);
    if (!dp) {
        return;
    }
    while ((ent = readdir(dp)) != NULL) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        unsigned long long size_bytes;
        int is_safetensors;
        int is_sidecar;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, ent->d_name)) {
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        size_bytes = qwen_source_stat_size_bytes(&st);
        is_safetensors = qwen_source_ends_with(ent->d_name, ".safetensors");
        is_sidecar = qwen_source_is_sidecar_file(ent->d_name);

        report->source_file_count++;
        report->source_regular_file_count++;
        report->total_size_bytes += size_bytes;

        if (is_safetensors) {
            report->safetensors_count++;
            report->safetensors_size_bytes += size_bytes;
        } else if (is_sidecar) {
            report->sidecar_size_bytes += size_bytes;
        } else {
            report->other_size_bytes += size_bytes;
        }
        if (qwen_source_ends_with(ent->d_name, ".bin")) {
            report->bin_count++;
        }
        if (qwen_source_ends_with(ent->d_name, ".dat")) {
            report->dat_count++;
        }
        if (qwen_source_ends_with(ent->d_name, ".json")) {
            report->json_count++;
        }
        if (qwen_source_is_tokenizer_file(ent->d_name)) {
            report->tokenizer_file_count++;
        }
        if (qwen_source_is_config_file(ent->d_name)) {
            report->config_file_count++;
        }
        if (size_bytes > report->largest_source_file_bytes) {
            report->largest_source_file_bytes = size_bytes;
            snprintf(report->largest_source_file_name,
                     sizeof(report->largest_source_file_name),
                     "%s",
                     ent->d_name);
        }
    }
    closedir(dp);
}

static unsigned long long qwen_source_native_tensor_elements(
    const yvex_native_weight_info *info)
{
    unsigned long long elements = 1;
    unsigned int i;

    if (!info) {
        return 0;
    }
    for (i = 0; i < info->rank; ++i) {
        if (info->dims[i] == 0) {
            return 0;
        }
        if (elements > ULLONG_MAX / info->dims[i]) {
            return ULLONG_MAX;
        }
        elements *= info->dims[i];
    }
    return elements;
}

static void qwen_source_tensor_shape_string(const yvex_native_weight_info *info,
                                            char *out,
                                            size_t cap)
{
    size_t used = 0;
    unsigned int i;

    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!info) {
        snprintf(out, cap, "[]");
        return;
    }
    used += (size_t)snprintf(out + used, cap - used, "[");
    for (i = 0; i < info->rank && used < cap; ++i) {
        int n = snprintf(out + used, cap - used, "%s%llu",
                         i == 0 ? "" : ",",
                         info->dims[i]);
        if (n < 0) {
            break;
        }
        if ((size_t)n >= cap - used) {
            out[cap - 1] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used < cap) {
        snprintf(out + used, cap - used, "]");
    } else {
        out[cap - 1] = '\0';
    }
}

static int qwen_source_tensor_shape_same(const yvex_native_weight_info *a,
                                         const yvex_native_weight_info *b)
{
    unsigned int i;

    if (!a || !b || a->rank != b->rank) {
        return 0;
    }
    for (i = 0; i < a->rank; ++i) {
        if (a->dims[i] != b->dims[i]) {
            return 0;
        }
    }
    return 1;
}

static int qwen_source_tensor_shape_first_seen(const yvex_native_weight_table *table,
                                               unsigned long long index)
{
    unsigned long long i;

    if (!table || index >= table->count) {
        return 0;
    }
    for (i = 0; i < index; ++i) {
        if (qwen_source_tensor_shape_same(&table->items[i], &table->items[index])) {
            return 0;
        }
    }
    return 1;
}

static int qwen_source_native_shard_first_seen(const yvex_native_weight_table *table,
                                               unsigned long long index)
{
    unsigned long long i;
    const char *shard;

    if (!table || index >= table->count) {
        return 0;
    }
    shard = table->items[index].shard_path;
    for (i = 0; i < index; ++i) {
        if (strcmp(table->items[i].shard_path, shard) == 0) {
            return 0;
        }
    }
    return 1;
}

static unsigned long long qwen_source_native_shard_max_data_end(
    const yvex_native_weight_table *table,
    const char *shard)
{
    unsigned long long i;
    unsigned long long max_end = 0;

    if (!table || !shard) {
        return 0;
    }
    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].shard_path, shard) == 0 &&
            table->items[i].data_end > max_end) {
            max_end = table->items[i].data_end;
        }
    }
    return max_end;
}

static int qwen_source_name_contains_ci(const char *name, const char *needle)
{
    size_t needle_len;
    size_t i;

    if (!name || !needle) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return 1;
    }
    for (i = 0; name[i] != '\0'; ++i) {
        size_t j;
        for (j = 0; j < needle_len; ++j) {
            if (name[i + j] == '\0' ||
                tolower((unsigned char)name[i + j]) !=
                    tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static const char *qwen_source_file_label(const char *path)
{
    const char *slash;

    if (!path || path[0] == '\0') {
        return "unknown";
    }
    slash = strrchr(path, '/');
    return slash && slash[1] != '\0' ? slash + 1 : path;
}

static void qwen_source_native_count_dtype(yvex_source_report *report,
                                           yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F16:
        report->native_dtype_f16_count++;
        break;
    case YVEX_NATIVE_DTYPE_BF16:
        report->native_dtype_bf16_count++;
        break;
    case YVEX_NATIVE_DTYPE_F32:
        report->native_dtype_f32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I8:
        report->native_dtype_i8_count++;
        break;
    case YVEX_NATIVE_DTYPE_I16:
        report->native_dtype_i16_count++;
        break;
    case YVEX_NATIVE_DTYPE_I32:
        report->native_dtype_i32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I64:
        report->native_dtype_i64_count++;
        break;
    case YVEX_NATIVE_DTYPE_U8:
        report->native_dtype_u8_count++;
        break;
    default:
        report->native_dtype_other_count++;
        break;
    }
}

static void qwen_source_metadata_count_dtype(yvex_source_report *report,
                                             yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F16:
        report->source_tensor_dtype_f16_count++;
        break;
    case YVEX_NATIVE_DTYPE_BF16:
        report->source_tensor_dtype_bf16_count++;
        break;
    case YVEX_NATIVE_DTYPE_F32:
        report->source_tensor_dtype_f32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I8:
        report->source_tensor_dtype_i8_count++;
        break;
    case YVEX_NATIVE_DTYPE_I16:
        report->source_tensor_dtype_i16_count++;
        break;
    case YVEX_NATIVE_DTYPE_I32:
        report->source_tensor_dtype_i32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I64:
        report->source_tensor_dtype_i64_count++;
        break;
    case YVEX_NATIVE_DTYPE_U8:
        report->source_tensor_dtype_u8_count++;
        break;
    default:
        report->source_tensor_dtype_other_count++;
        break;
    }
}

static void qwen_source_metadata_count_rank(yvex_source_report *report,
                                            unsigned long long rank)
{
    if (rank == 0) report->source_tensor_rank_0_count++;
    else if (rank == 1) report->source_tensor_rank_1_count++;
    else if (rank == 2) report->source_tensor_rank_2_count++;
    else if (rank == 3) report->source_tensor_rank_3_count++;
    else if (rank == 4) report->source_tensor_rank_4_count++;
    else report->source_tensor_rank_other_count++;
}

static void qwen_source_metadata_count_name(yvex_source_report *report,
                                            const char *name)
{
    if (qwen_source_name_contains_ci(name, "embed") ||
        qwen_source_name_contains_ci(name, "embd")) {
        report->source_tensor_name_embed_count++;
    } else if (qwen_source_name_contains_ci(name, "attn") ||
               qwen_source_name_contains_ci(name, "attention")) {
        report->source_tensor_name_attn_count++;
    } else if (qwen_source_name_contains_ci(name, "mlp") ||
               qwen_source_name_contains_ci(name, "ffn") ||
               qwen_source_name_contains_ci(name, "feed_forward")) {
        report->source_tensor_name_mlp_count++;
    } else if (qwen_source_name_contains_ci(name, "norm")) {
        report->source_tensor_name_norm_count++;
    } else if (qwen_source_name_contains_ci(name, "lm_head") ||
               qwen_source_name_contains_ci(name, "output.weight")) {
        report->source_tensor_name_lm_head_count++;
    } else {
        report->source_tensor_name_other_count++;
    }
}

static unsigned long long qwen_source_metadata_count_distinct_dtypes(
    const yvex_source_report *report)
{
    unsigned long long count = 0;

    if (!report) {
        return 0;
    }
    if (report->source_tensor_dtype_f16_count) count++;
    if (report->source_tensor_dtype_bf16_count) count++;
    if (report->source_tensor_dtype_f32_count) count++;
    if (report->source_tensor_dtype_i8_count) count++;
    if (report->source_tensor_dtype_i16_count) count++;
    if (report->source_tensor_dtype_i32_count) count++;
    if (report->source_tensor_dtype_i64_count) count++;
    if (report->source_tensor_dtype_u8_count) count++;
    if (report->source_tensor_dtype_other_count) count++;
    return count;
}

static unsigned long long qwen_source_metadata_count_distinct_ranks(
    const yvex_source_report *report)
{
    unsigned long long count = 0;

    if (!report) {
        return 0;
    }
    if (report->source_tensor_rank_0_count) count++;
    if (report->source_tensor_rank_1_count) count++;
    if (report->source_tensor_rank_2_count) count++;
    if (report->source_tensor_rank_3_count) count++;
    if (report->source_tensor_rank_4_count) count++;
    if (report->source_tensor_rank_other_count) count++;
    return count;
}

static void qwen_source_metadata_add_sample(
    yvex_source_report *report,
    const yvex_native_weight_info *info,
    unsigned long long elements)
{
    yvex_source_tensor_sample *sample;

    if (!report || !info ||
        report->source_tensor_sample_count >= YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        return;
    }
    sample = &report->source_tensor_samples[report->source_tensor_sample_count++];
    snprintf(sample->name, sizeof(sample->name), "%s", info->name ? info->name : "unknown");
    snprintf(sample->file, sizeof(sample->file), "%s", qwen_source_file_label(info->shard_path));
    snprintf(sample->dtype, sizeof(sample->dtype), "%s", yvex_native_dtype_name(info->dtype));
    qwen_source_tensor_shape_string(info, sample->shape, sizeof(sample->shape));
    sample->rank = info->rank;
    sample->elements = elements;
    sample->declared_bytes = info->data_bytes;
}

static void qwen_source_native_collect_table(
    yvex_source_report *report,
    const yvex_native_weight_table *table)
{
    unsigned long long i;

    if (!report || !table) {
        return;
    }
    report->native_safetensors_header_read_count = table->header_read_count;
    report->native_safetensors_header_error_count = table->header_error_count;
    report->native_safetensors_header_bytes = table->header_bytes;
    report->native_tensor_count = table->count;
    report->native_declared_tensor_bytes = table->summary.total_tensor_bytes;
    report->native_invalid_file_count = table->header_error_count;
    report->native_inventory_error_count = table->header_error_count;
    report->source_tensor_count = table->count;
    report->source_tensor_name_count = table->count;
    report->source_tensor_declared_tensor_bytes = table->summary.total_tensor_bytes;
    report->source_tensor_metadata_error_count = table->header_error_count;

    for (i = 0; i < table->count; ++i) {
        const yvex_native_weight_info *info = &table->items[i];
        unsigned long long elements = qwen_source_native_tensor_elements(info);
        char shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];

        qwen_source_native_count_dtype(report, info->dtype);
        qwen_source_metadata_count_dtype(report, info->dtype);
        qwen_source_metadata_count_rank(report, info->rank);
        qwen_source_metadata_count_name(report, info->name);
        if (elements != ULLONG_MAX &&
            report->source_tensor_total_elements <= ULLONG_MAX - elements) {
            report->source_tensor_total_elements += elements;
        } else {
            report->source_tensor_total_elements = ULLONG_MAX;
        }
        if (info->rank > report->native_max_rank) {
            report->native_max_rank = info->rank;
        }
        if (info->rank > report->source_tensor_max_rank) {
            report->source_tensor_max_rank = info->rank;
        }
        if (elements > report->native_max_tensor_elements) {
            report->native_max_tensor_elements = elements;
        }
        if (elements > report->source_tensor_max_elements) {
            report->source_tensor_max_elements = elements;
        }
        if (qwen_source_tensor_shape_first_seen(table, i)) {
            report->source_tensor_shape_count++;
        }
        qwen_source_metadata_add_sample(report, info, elements);
        if (info->data_bytes > report->native_largest_tensor_bytes) {
            report->native_largest_tensor_bytes = info->data_bytes;
            snprintf(report->native_largest_tensor_name,
                     sizeof(report->native_largest_tensor_name),
                     "%s",
                     info->name ? info->name : "unknown");
        }
        if (info->data_bytes > report->source_tensor_largest_declared_bytes) {
            qwen_source_tensor_shape_string(info, shape, sizeof(shape));
            report->source_tensor_largest_declared_bytes = info->data_bytes;
            report->source_tensor_largest_rank = info->rank;
            report->source_tensor_largest_elements = elements;
            snprintf(report->source_tensor_largest_name,
                     sizeof(report->source_tensor_largest_name),
                     "%s",
                     info->name ? info->name : "unknown");
            snprintf(report->source_tensor_largest_file,
                     sizeof(report->source_tensor_largest_file),
                     "%s",
                     qwen_source_file_label(info->shard_path));
            snprintf(report->source_tensor_largest_dtype,
                     sizeof(report->source_tensor_largest_dtype),
                     "%s",
                     yvex_native_dtype_name(info->dtype));
            snprintf(report->source_tensor_largest_shape,
                     sizeof(report->source_tensor_largest_shape),
                     "%s",
                     shape);
        }
        if (qwen_source_native_shard_first_seen(table, i)) {
            unsigned long long shard_bytes =
                qwen_source_native_shard_max_data_end(table, info->shard_path);
            report->native_declared_data_bytes += shard_bytes;
            report->source_tensor_declared_data_bytes += shard_bytes;
            report->source_tensor_file_count++;
        }
    }
    report->source_tensor_dtype_count =
        qwen_source_metadata_count_distinct_dtypes(report);
    report->source_tensor_rank_count =
        qwen_source_metadata_count_distinct_ranks(report);
}

static int qwen_source_scan_native_inventory(const char *dir,
                                             yvex_source_report *report)
{
    const unsigned long long max_safetensors_files = 1024;
    yvex_native_weight_table *table;
    DIR *dp;
    struct dirent *ent;
    int fatal_rc = YVEX_OK;
    unsigned long long scan_error_count = 0;

    if (!dir || !report || !report->source_exists) {
        return YVEX_OK;
    }
    table = (yvex_native_weight_table *)calloc(1, sizeof(*table));
    if (!table) {
        return YVEX_ERR_NOMEM;
    }
    dp = opendir(dir);
    if (!dp) {
        free(table);
        return YVEX_OK;
    }
    while ((ent = readdir(dp)) != NULL) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        yvex_error err;
        int rc;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0 ||
            !qwen_source_ends_with(ent->d_name, ".safetensors")) {
            continue;
        }
        if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, ent->d_name)) {
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (report->native_safetensors_count >= max_safetensors_files) {
            scan_error_count++;
            break;
        }

        report->native_safetensors_count++;
        report->native_safetensors_opened++;
        table->summary.shard_count++;
        yvex_error_clear(&err);
        {
            unsigned long long error_count_before = table->header_error_count;
            rc = yvex_safetensors_read_header_file(path, ent->d_name, table, &err);
            if (rc != YVEX_OK && table->header_error_count == error_count_before) {
                table->header_error_count++;
            }
        }
        if (rc == YVEX_ERR_NOMEM) {
            fatal_rc = YVEX_ERR_NOMEM;
            break;
        }
    }
    closedir(dp);
    qwen_source_native_collect_table(report, table);
    report->native_safetensors_header_error_count += scan_error_count;
    report->native_invalid_file_count += scan_error_count;
    report->native_inventory_error_count += scan_error_count;
    report->source_tensor_metadata_error_count += scan_error_count;
    yvex_native_weight_table_close(table);
    return fatal_rc;
}

static void qwen_source_choose_report_file(char *out, size_t cap,
                                           const yvex_source_family_profile *profile,
                                           const char *reports_root,
                                           const char *source_path,
                                           const char *target,
                                           const char *kind,
                                           int *out_exists)
{
    char candidate[YVEX_PATH_CAP];
    char file_name[96];
    int n;

    if (out_exists) {
        *out_exists = 0;
    }
    if (!out || cap == 0 || !profile || !kind) {
        return;
    }
    out[0] = '\0';

    if (source_path && source_path[0] != '\0') {
        if (strcmp(kind, "manifest") == 0 &&
            (qwen_source_manifest_file_exists(out, cap, source_path, "source_manifest.json") ||
             qwen_source_manifest_file_exists(out, cap, source_path, "source-manifest.json"))) {
            if (out_exists) *out_exists = 1;
            return;
        }
        if (strcmp(kind, "manifest") != 0 &&
            qwen_source_manifest_file_exists(out, cap, source_path, "native-inventory.json")) {
            if (out_exists) *out_exists = 1;
            return;
        }
    }

    if (reports_root && reports_root[0] != '\0') {
        {
            char target_prefix[YVEX_PATH_CAP];

            n = snprintf(target_prefix, sizeof(target_prefix), "%s/%s/%s",
                         reports_root, profile->family_key, target);
            if (n >= 0 && (size_t)n < sizeof(target_prefix) &&
                qwen_source_path_format(candidate, sizeof(candidate), "%s.%s",
                                        target_prefix,
                                        strcmp(kind, "manifest") == 0
                                            ? "source-manifest.json"
                                            : "native-inventory.json") &&
                qwen_source_stat_kind(candidate, 0)) {
                snprintf(out, cap, "%s", candidate);
                if (out_exists) *out_exists = 1;
                return;
            }
        }
        n = snprintf(file_name, sizeof(file_name), "%s-%s",
                     profile->family_key,
                     strcmp(kind, "manifest") == 0
                         ? "source-manifest.json"
                         : "native-inventory.json");
        if (n < 0 || (size_t)n >= sizeof(file_name)) {
            return;
        }
        n = snprintf(candidate, sizeof(candidate), "%s/%s/%s",
                     reports_root, profile->family_key, file_name);
        if (n >= 0 && (size_t)n < sizeof(candidate) &&
            qwen_source_stat_kind(candidate, 0)) {
            snprintf(out, cap, "%s", candidate);
            if (out_exists) *out_exists = 1;
            return;
        }
        n = snprintf(out, cap, "%s/%s/%s.%s",
                     reports_root,
                     profile->family_key,
                     target,
                     strcmp(kind, "manifest") == 0
                         ? "source-manifest.json"
                         : "native-inventory.json");
        if (n < 0 || (size_t)n >= cap) {
            out[cap - 1] = '\0';
        }
    }
}

static int qwen_source_manifest_blob_has_field(const char *blob, const char *field)
{
    char quoted[96];
    int n;

    if (!blob || !field) {
        return 0;
    }
    n = snprintf(quoted, sizeof(quoted), "\"%s\"", field);
    if (n < 0 || (size_t)n >= sizeof(quoted)) {
        return 0;
    }
    return strstr(blob, quoted) != NULL;
}

static int qwen_source_manifest_blob_has_value(const char *blob, const char *value)
{
    return blob && value && value[0] != '\0' && strstr(blob, value) != NULL;
}

static void qwen_source_probe_manifest(yvex_source_report *report,
                                       const yvex_source_family_profile *profile,
                                       const char *target)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    FILE *fp;
    size_t nread;

    if (!report || !profile || !target || !report->manifest_exists ||
        report->manifest_path[0] == '\0') {
        return;
    }
    fp = fopen(report->manifest_path, "rb");
    if (!fp) {
        report->manifest_probe_error = 1;
        return;
    }
    nread = fread(buf, 1, YVEX_SOURCE_MANIFEST_PROBE_CAP, fp);
    if (ferror(fp)) {
        report->manifest_probe_error = 1;
        fclose(fp);
        return;
    }
    fclose(fp);
    buf[nread] = '\0';
    report->manifest_probe_checked = 1;
    report->manifest_has_schema =
        qwen_source_manifest_blob_has_field(buf, "schema");
    report->manifest_schema_matches =
        qwen_source_manifest_blob_has_value(buf, "yvex.source_manifest.v1");
    snprintf(report->manifest_schema_version,
             sizeof(report->manifest_schema_version),
             "%s",
             report->manifest_schema_matches ? "yvex.source_manifest.v1" : "unknown");
    report->manifest_has_family =
        qwen_source_manifest_blob_has_field(buf, "family") ||
        qwen_source_manifest_blob_has_value(buf, profile->family_key) ||
        qwen_source_manifest_blob_has_value(buf, profile->display_family);
    report->manifest_family_matches =
        qwen_source_manifest_blob_has_value(buf, profile->family_key) ||
        qwen_source_manifest_blob_has_value(buf, profile->display_family);
    report->manifest_has_target =
        qwen_source_manifest_blob_has_field(buf, "target") ||
        qwen_source_manifest_blob_has_value(buf, target);
    report->manifest_target_matches =
        qwen_source_manifest_blob_has_value(buf, target);
    report->manifest_has_artifact_class =
        qwen_source_manifest_blob_has_field(buf, "artifact_class") ||
        qwen_source_manifest_blob_has_field(buf, "source_artifact_class") ||
        qwen_source_manifest_blob_has_value(buf, profile->source_artifact_class);
    report->manifest_has_footprint =
        qwen_source_manifest_blob_has_field(buf, "footprint") ||
        qwen_source_manifest_blob_has_field(buf, "summary");
    report->manifest_has_provenance =
        qwen_source_manifest_blob_has_field(buf, "provenance") ||
        qwen_source_manifest_blob_has_field(buf, "source");
    report->manifest_has_native_inventory =
        qwen_source_manifest_blob_has_field(buf, "native_inventory") ||
        qwen_source_manifest_blob_has_field(buf, "native");
    report->manifest_has_tensor_metadata =
        qwen_source_manifest_blob_has_field(buf, "tensor_metadata") ||
        qwen_source_manifest_blob_has_field(buf, "tensors");
}

static void qwen_source_add_blocker(yvex_source_report *report,
                                    const char *blocker)
{
    if (!report || !blocker || report->blocker_count >= 32) {
        return;
    }
    report->blockers[report->blocker_count++] = blocker;
}


static int qwen_source_tail_blocker_is_tensor_map(const char *blocker)
{
    return blocker &&
           (strstr(blocker, "tensor-role-map") != NULL ||
            strstr(blocker, "tensor-map") != NULL);
}

/*
 * source_report_apply_deepseek_verification()
 *
 * Projects exact source-verifier facts into the existing typed source report.
 * It allocates nothing, performs no IO, and does not promote artifact or
 * runtime capability.
 */
static void source_report_apply_deepseek_verification(
    yvex_source_report *report)
{
    const yvex_source_verification *verification = &report->verification;

    if (verification->repository_id[0]) {
        snprintf(report->identity_repo_id, sizeof(report->identity_repo_id), "%s",
                 verification->repository_id);
    }
    if (verification->revision[0]) {
        snprintf(report->identity_revision, sizeof(report->identity_revision), "%s",
                 verification->revision);
    }
    snprintf(report->identity_local_source_dir,
             sizeof(report->identity_local_source_dir), "%s",
             verification->resolved_source_path);
    snprintf(report->manifest_path, sizeof(report->manifest_path), "%s",
             verification->manifest_path);
    report->manifest_exists = verification->manifest_path[0] != '\0';
    report->source_identity_from_path = verification->path_verified;
    report->config_exists = verification->config_valid;
    report->tokenizer_json_exists = verification->tokenizer_json_valid;
    report->tokenizer_config_exists = verification->tokenizer_config_valid;
    report->source_file_count = verification->source_file_count;
    report->source_regular_file_count = verification->source_file_count;
    report->safetensors_count = verification->shard_count;
    report->total_size_bytes = verification->source_total_bytes;
    report->safetensors_size_bytes = verification->shard_bytes;
    report->sidecar_size_bytes = verification->source_total_bytes >=
                                         verification->shard_bytes
                                     ? verification->source_total_bytes -
                                           verification->shard_bytes
                                     : 0u;
    report->native_safetensors_count = verification->shard_count;
    report->native_safetensors_opened = verification->shard_count;
    report->native_safetensors_header_read_count =
        verification->header_shard_count;
    report->native_safetensors_header_error_count =
        verification->header_shard_count <= verification->shard_count
            ? verification->shard_count - verification->header_shard_count
            : 0u;
    report->native_safetensors_header_bytes = verification->header_bytes;
    report->native_tensor_count = verification->header_tensor_count;
    report->native_declared_tensor_bytes = verification->declared_tensor_bytes;
    report->native_max_rank = verification->max_tensor_rank;
    report->native_dtype_f16_count = verification->dtype_f16_count;
    report->native_dtype_bf16_count = verification->dtype_bf16_count;
    report->native_dtype_f32_count = verification->dtype_f32_count;
    report->native_dtype_i64_count = verification->dtype_i64_count;
    report->native_dtype_i8_count = verification->dtype_i8_count;
    report->native_dtype_other_count = verification->dtype_fp4_count +
                                       verification->dtype_f8_count +
                                       verification->dtype_f8_e8m0_count +
                                       verification->dtype_other_count;
    report->source_tensor_count = verification->header_tensor_count;
    report->source_tensor_name_count = verification->header_tensor_count;
    report->source_tensor_file_count = verification->header_shard_count;
    report->source_tensor_declared_tensor_bytes =
        verification->declared_tensor_bytes;
    report->source_tensor_max_rank = verification->max_tensor_rank;
    report->source_tensor_dtype_f16_count = verification->dtype_f16_count;
    report->source_tensor_dtype_bf16_count = verification->dtype_bf16_count;
    report->source_tensor_dtype_f32_count = verification->dtype_f32_count;
    report->source_tensor_dtype_i64_count = verification->dtype_i64_count;
    report->source_tensor_dtype_i8_count = verification->dtype_i8_count;
    report->source_tensor_dtype_other_count =
        verification->dtype_fp4_count + verification->dtype_f8_count +
        verification->dtype_f8_e8m0_count + verification->dtype_other_count;
    report->source_tensor_dtype_count =
        (verification->dtype_f16_count ? 1u : 0u) +
        (verification->dtype_bf16_count ? 1u : 0u) +
        (verification->dtype_f32_count ? 1u : 0u) +
        (verification->dtype_i64_count ? 1u : 0u) +
        (verification->dtype_i8_count ? 1u : 0u) +
        (verification->dtype_fp4_count ? 1u : 0u) +
        (verification->dtype_f8_count ? 1u : 0u) +
        (verification->dtype_f8_e8m0_count ? 1u : 0u) +
        (verification->dtype_other_count ? 1u : 0u);
}

int yvex_source_report_build(const yvex_source_report_request *request,
                             yvex_source_report *report,
                             yvex_error *err)
{
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    int rc;
    int n;
    int deepseek;
    unsigned long i;

    const yvex_source_report_request *options = request;

    if (!options || !report || !options->profile) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_report_build",
                       "request, profile, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->target ||
        !yvex_source_report_target_is_supported(options->profile, options->target)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source_report_build",
                        "unsupported target: %s",
                        options->target ? options->target : "");
        return YVEX_ERR_INVALID_ARG;
    }
    deepseek = yvex_model_target_is_release_target(options->target);

    memset(report, 0, sizeof(*report));
    report->profile = options->profile;
    report->request = *options;
    if (options->target == options->resolved_target) {
        report->request.target = report->request.resolved_target;
    }
    yvex_error_clear(err);
    rc = yvex_paths_default(&paths, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_operator_paths_resolve(&paths, options->models_root, &operator_paths, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (options->source) {
        if (!qwen_source_path_format(report->source_path, sizeof(report->source_path),
                                     "%s", options->source, NULL)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "source_report_build",
                           "source path is too long");
            return YVEX_ERR_BOUNDS;
        }
        snprintf(report->source_path_source, sizeof(report->source_path_source),
                 "%s", "explicit-source");
    } else {
        if (deepseek) {
            n = yvex_model_target_source_path(
                report->source_path, sizeof(report->source_path),
                operator_paths.models_root,
                yvex_model_target_release_identity()) ? 0 : -1;
        } else {
            n = snprintf(report->source_path, sizeof(report->source_path),
                         "%s/hf/%s/%s",
                         operator_paths.models_root,
                         options->profile->family_key,
                         options->target);
        }
        if (n < 0 || (!deepseek && (size_t)n >= sizeof(report->source_path))) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "source_report_build",
                           "source path is too long");
            return YVEX_ERR_BOUNDS;
        }
        snprintf(report->source_path_source, sizeof(report->source_path_source),
                 "%s", operator_paths.models_root_source);
    }
    snprintf(report->identity_target_id, sizeof(report->identity_target_id),
             "%s", options->target);
    snprintf(report->identity_family, sizeof(report->identity_family),
             "%s", options->profile->family_key);
    if (deepseek) {
        const yvex_model_target_identity *identity =
            yvex_model_target_release_identity();

        snprintf(report->identity_model, sizeof(report->identity_model), "%s",
                 identity->model_name);
        snprintf(report->identity_repo_id, sizeof(report->identity_repo_id), "%s",
                 identity->upstream_repo_id);
    }
    if (options->source) {
        const char *base = qwen_source_path_basename(report->source_path);
        if (base && strcmp(base, options->target) == 0 &&
            qwen_source_target_matches_family_name(options->profile->family_key, base)) {
            report->source_identity_from_path = 1;
        }
    }
    if (!report->identity_model[0] &&
        strcmp(options->target, options->profile->target_id) != 0) {
        snprintf(report->identity_model, sizeof(report->identity_model), "%s",
                 options->target);
    }
    {
        char registry_family_dir[YVEX_PATH_CAP];
        char reports_family_dir[YVEX_PATH_CAP];
        char file_name[192];

        if (qwen_source_path_format(registry_family_dir, sizeof(registry_family_dir),
                                    "%s/%s",
                                    operator_paths.registry_root,
                                    options->profile->family_key) &&
            qwen_source_path_format(reports_family_dir, sizeof(reports_family_dir),
                                    "%s/%s",
                                    operator_paths.reports_root,
                                    options->profile->family_key)) {
            snprintf(file_name, sizeof(file_name), "%s.download.json", options->target);
            (void)qwen_source_path_format(report->download_registry_path,
                                          sizeof(report->download_registry_path),
                                          "%s/%s", registry_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.download-report.json", options->target);
            (void)qwen_source_path_format(report->download_report_path,
                                          sizeof(report->download_report_path),
                                          "%s/%s", reports_family_dir, file_name);
            report->download_registry_exists =
                report->download_registry_path[0] &&
                access(report->download_registry_path, F_OK) == 0;
            report->download_report_exists =
                report->download_report_path[0] &&
                access(report->download_report_path, F_OK) == 0;
            snprintf(file_name, sizeof(file_name), "%s.tensor-map.json", options->target);
            (void)qwen_source_path_format(report->tensor_map_path,
                                          sizeof(report->tensor_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.output-head-map.json", options->target);
            (void)qwen_source_path_format(report->output_head_map_path,
                                          sizeof(report->output_head_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.tokenizer-map.json", options->target);
            (void)qwen_source_path_format(report->tokenizer_map_path,
                                          sizeof(report->tokenizer_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            report->tensor_map_exists =
                report->tensor_map_path[0] &&
                access(report->tensor_map_path, F_OK) == 0;
            report->output_head_map_exists =
                report->output_head_map_path[0] &&
                access(report->output_head_map_path, F_OK) == 0;
            report->tokenizer_map_exists =
                report->tokenizer_map_path[0] &&
                access(report->tokenizer_map_path, F_OK) == 0;
            qwen_source_probe_map_sidecars(report);
            if (!deepseek && report->download_registry_exists) {
                (void)qwen_source_probe_download_identity_file(
                    report->download_registry_path,
                    options->target,
                    options->profile->family_key,
                    report);
            }
            if (!deepseek && !report->source_identity_from_download_sidecar &&
                report->download_report_exists) {
                (void)qwen_source_probe_download_identity_file(
                    report->download_report_path,
                    options->target,
                    options->profile->family_key,
                    report);
            }
        }
    }

    report->source_exists = qwen_source_stat_kind(report->source_path, 1);
    if (report->source_exists) {
        report->config_exists = qwen_source_check_file(report->source_path, "config.json");
        report->generation_config_exists =
            qwen_source_check_file(report->source_path, "generation_config.json");
        report->tokenizer_json_exists =
            qwen_source_check_file(report->source_path, "tokenizer.json");
        report->tokenizer_config_exists =
            qwen_source_check_file(report->source_path, "tokenizer_config.json");
        report->readme_exists = qwen_source_check_file(report->source_path, "README.md");
        report->license_exists =
            qwen_source_check_file(report->source_path, "LICENSE") ||
            qwen_source_check_file(report->source_path, "LICENSE.txt") ||
            qwen_source_check_file(report->source_path, "COPYING");
        if (deepseek) {
            yvex_source_verify_options verify_options;

            memset(&verify_options, 0, sizeof(verify_options));
            verify_options.identity = yvex_model_target_release_identity();
            verify_options.source_path = report->source_path;
            verify_options.models_root = operator_paths.models_root;
            rc = yvex_source_verify(&verify_options, &report->verification, err);
            if (rc != YVEX_OK) return rc;
            source_report_apply_deepseek_verification(report);
        } else {
            qwen_source_scan_top_footprint(report->source_path, report);
            rc = qwen_source_scan_native_inventory(report->source_path, report);
            if (rc != YVEX_OK) {
                return rc;
            }
        }
    }
    if (deepseek && !report->source_exists) {
        yvex_source_verify_options verify_options;

        memset(&verify_options, 0, sizeof(verify_options));
        verify_options.identity = yvex_model_target_release_identity();
        verify_options.source_path = report->source_path;
        verify_options.models_root = operator_paths.models_root;
        rc = yvex_source_verify(&verify_options, &report->verification, err);
        if (rc != YVEX_OK) return rc;
        source_report_apply_deepseek_verification(report);
    }

    if (!deepseek) {
        qwen_source_choose_report_file(report->manifest_path,
                                       sizeof(report->manifest_path),
                                       options->profile,
                                       operator_paths.reports_root,
                                       report->source_path,
                                       options->target,
                                       "manifest",
                                       &report->manifest_exists);
        qwen_source_choose_report_file(report->native_inventory_path,
                                       sizeof(report->native_inventory_path),
                                       options->profile,
                                       operator_paths.reports_root,
                                       report->source_path,
                                       options->target,
                                       "inventory",
                                       &report->native_inventory_exists);
        qwen_source_probe_manifest(report, options->profile, options->target);
    }

    report->source_state = report->source_exists ? "present" : "missing";
    if (deepseek) {
        report->status = report->verification.verified
                             ? "exact-source-verified"
                             : "exact-source-blocked";
        report->top_blocker = report->verification.blocker_count
                                  ? report->verification.blockers[0]
                                  : "none";
        report->next_row = report->verification.verified
                               ? "V010.GGUF.QTYPE.ABI.1"
                               : "V010.REBASE.DEEPSEEK.0";
        for (i = 0; i < report->verification.blocker_count; ++i) {
            qwen_source_add_blocker(report,
                                    report->verification.blockers[i]);
        }
        for (i = 0; i < options->profile->tail_blocker_count; ++i) {
            qwen_source_add_blocker(report,
                                    options->profile->tail_blockers[i]);
        }
        report->exit_code = options->strict && !report->verification.verified
                                ? 5
                                : 0;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!report->source_exists) {
        report->status = "source-target-profiled";
        report->top_blocker = options->profile->source_manifest_blocker;
        report->next_row = options->profile->model_class_next;
    } else if (!report->manifest_exists) {
        report->status = "source-present-report-only";
        report->top_blocker = options->profile->source_manifest_blocker;
        report->next_row = options->profile->model_class_next;
    } else {
        report->status = "source-profile-incomplete";
        report->top_blocker = options->profile->model_class_blocker;
        report->next_row = options->profile->model_class_next;
    }
    if (report->source_exists &&
        report->tensor_map_exists &&
        !report->tensor_map_incomplete &&
        report->output_head_map_exists &&
        !report->output_head_map_missing) {
        if (report->tokenizer_map_exists) {
            report->top_blocker = "quant-policy-or-artifact-emitter";
            report->next_row = "V010.QUANT.1";
        } else {
            report->top_blocker = options->profile->tokenizer_blocker;
            report->next_row = "V010.MAP.7";
        }
    }

    if (!report->source_exists) {
        qwen_source_add_blocker(report, options->profile->source_path_blocker);
    }
    if (!report->manifest_exists) {
        qwen_source_add_blocker(report, options->profile->source_manifest_blocker);
    }
    if (!report->config_exists) {
        qwen_source_add_blocker(report, options->profile->source_config_blocker);
    }
    if (!(report->tokenizer_json_exists || report->tokenizer_config_exists)) {
        qwen_source_add_blocker(report, options->profile->tokenizer_blocker);
    }
    for (i = 0; i < options->profile->tail_blocker_count; ++i) {
        if (report->source_exists &&
            report->tensor_map_exists &&
            !report->tensor_map_incomplete &&
            qwen_source_tail_blocker_is_tensor_map(
                options->profile->tail_blockers[i])) {
            continue;
        }
        qwen_source_add_blocker(report, options->profile->tail_blockers[i]);
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
