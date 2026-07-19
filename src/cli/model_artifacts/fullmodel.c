/* Owner: src/cli/render
 * Owns: typed option parsing, inventory classification, descriptor and materialization diagnostics, report
 *   rendering, and help projection.
 * Does not own: runtime generation, graph execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing fullmodel behavior.
 * Boundary: fullmodel reports are diagnostic/report-only unless a lower layer proves otherwise.
 * Purpose: provide typed option parsing, inventory classification, descriptor and materialization diagnostics,
 *   report rendering, and help projection.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/model_artifacts/private.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char name[32];
    unsigned long long count;
    unsigned long long bytes;
} fullmodel_dtype_bucket;

/* Purpose: Compute fullmodel file size for its CLI invariant (`fullmodel_file_size`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_file_size(const char *path,
                               unsigned long long *bytes)
{
    struct stat st;

    if (bytes) *bytes = 0ull;
    if (!path || stat(path, &st) != 0) return 0;
    if (bytes) *bytes = (unsigned long long)st.st_size;
    return 1;
}

/* Purpose: Compute fullmodel family from arch for its CLI invariant (`fullmodel_family_from_arch`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *fullmodel_family_from_arch(yvex_arch arch)
{
    switch (arch) {
    case YVEX_ARCH_DEEPSEEK: return "deepseek";
    case YVEX_ARCH_GLM: return "glm";
    case YVEX_ARCH_LLAMA: return "llama";
    case YVEX_ARCH_QWEN: return "qwen";
    case YVEX_ARCH_GEMMA: return "gemma";
    case YVEX_ARCH_PHI: return "phi";
    case YVEX_ARCH_KIMI: return "kimi";
    default: return "unknown";
    }
}

static const char *const literal_pair_0[] = { "fullmodel: report",
    "status: fullmodel-report"};

/* Purpose: Compute fullmodel name has for its CLI invariant (`fullmodel_name_has`). */
static int fullmodel_name_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

/* Purpose: Compute fullmodel csv append for its CLI invariant (`fullmodel_csv_append`). */
static void fullmodel_csv_append(char *buf,
                                 size_t cap,
                                 const char *item)
{
    size_t used;
    int n;

    if (!buf || cap == 0u || !item || !item[0]) return;
    used = strlen(buf);
    if (used >= cap - 1u) return;
    n = snprintf(buf + used, cap - used, "%s%s", used == 0u ? "" : ",", item);
    if (n < 0 || (size_t)n >= cap - used) buf[cap - 1u] = '\0';
}

/* Purpose: Compute fullmodel collection add for its CLI invariant (`fullmodel_collection_add`). */
static void fullmodel_collection_add(unsigned long long *count,
                                     unsigned long long *bytes,
                                     const yvex_tensor_info *tensor)
{
    if (count) (*count)++;
    if (bytes && tensor) *bytes += tensor->storage_bytes;
}

typedef struct tensor_collection_rule {
    yvex_tensor_role role;
    const char *name_a;
    const char *name_b;
    int names_must_both_match;
    int name_a_must_equal;
    size_t count_offset;
    size_t bytes_offset;
    size_t flag_offset;
} tensor_collection_rule;

#define COLLECTION_RULE(role_, a_, b_, both_, exact_, count_, flag_) \
    {role_, a_, b_, both_, exact_, offsetof(yvex_fullmodel_collections, count_), \
     offsetof(yvex_fullmodel_collections, count_##_bytes), \
     offsetof(yvex_fullmodel_collections, flag_)}

static const tensor_collection_rule typed_collection_rules[] = {
    COLLECTION_RULE(YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, NULL, NULL, 0, 0,
                    embedding, has_token_embedding),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_OUTPUT_NORM, NULL, NULL, 0, 0,
                    normalization, has_output_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_OUTPUT_HEAD, NULL, NULL, 0, 0,
                    output, has_output_head),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_ATTENTION_NORM, NULL, NULL, 0, 0,
                    normalization, has_attention_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_ATTENTION_Q, NULL, NULL, 0, 0,
                    attention, has_attention_q),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_ATTENTION_K, NULL, NULL, 0, 0,
                    attention, has_attention_k),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_ATTENTION_V, NULL, NULL, 0, 0,
                    attention, has_attention_v),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_ATTENTION_OUT, NULL, NULL, 0, 0,
                    attention, has_attention_out),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_FFN_NORM, NULL, NULL, 0, 0,
                    normalization, has_post_attention_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_FFN_GATE, NULL, NULL, 0, 0, mlp, has_ffn_gate),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_FFN_UP, NULL, NULL, 0, 0, mlp, has_ffn_up),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_FFN_DOWN, NULL, NULL, 0, 0, mlp, has_ffn_down),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_MOE_ROUTER, NULL, NULL, 0, 0, moe, has_moe_router),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, NULL, NULL, 0, 0,
                    moe, has_moe_expert),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_MOE_EXPERT_UP, NULL, NULL, 0, 0, moe, has_moe_expert),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, NULL, NULL, 0, 0,
                    moe, has_moe_expert),
};

static const tensor_collection_rule named_collection_rules[] = {
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "token_embd", "embed", 0, 0,
                    embedding, has_token_embedding),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "attn_norm", "input_layernorm", 0, 0,
                    normalization, has_attention_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "ffn_norm", "post_attention_layernorm", 0, 0,
                    normalization, has_post_attention_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "attn_q", "q_proj", 0, 0,
                    attention, has_attention_q),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "attn_k", "k_proj", 0, 0,
                    attention, has_attention_k),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "attn_v", "v_proj", 0, 0,
                    attention, has_attention_v),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "attn_output", "o_proj", 0, 0,
                    attention, has_attention_out),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "ffn_gate", "gate_proj", 0, 0,
                    mlp, has_ffn_gate),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "ffn_up", "up_proj", 0, 0, mlp, has_ffn_up),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "ffn_down", "down_proj", 0, 0,
                    mlp, has_ffn_down),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "router", "gate.weight", 0, 0,
                    moe, has_moe_router),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "expert", NULL, 0, 0, moe, has_moe_expert),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "output_norm", "norm.weight", 0, 0,
                    normalization, has_output_norm),
    COLLECTION_RULE(YVEX_TENSOR_ROLE_UNKNOWN, "output.weight", "lm_head", 0, 1,
                    output, has_output_head),
};

typedef struct descriptor_tensor_rule {
    const char *logical_role;
    yvex_tensor_role typed_role;
    const char *name_a;
    const char *name_b;
    int names_must_both_match;
    int name_a_must_equal;
} descriptor_tensor_rule;

#define DESCRIPTOR_RULE(name_, role_, a_, b_, both_, exact_) \
    {name_, role_, a_, b_, both_, exact_}

static const descriptor_tensor_rule descriptor_tensor_rules[] = {
    DESCRIPTOR_RULE("token_embedding", YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
                    "token_embd", "embed", 0, 0),
    DESCRIPTOR_RULE("attention_norm", YVEX_TENSOR_ROLE_ATTENTION_NORM,
                    "attn_norm", "input_layernorm", 0, 0),
    DESCRIPTOR_RULE("post_attention_norm", YVEX_TENSOR_ROLE_FFN_NORM,
                    "ffn_norm", "post_attention_layernorm", 0, 0),
    DESCRIPTOR_RULE("final_norm", YVEX_TENSOR_ROLE_OUTPUT_NORM,
                    "output_norm", "final_norm", 0, 0),
    DESCRIPTOR_RULE("q_projection", YVEX_TENSOR_ROLE_ATTENTION_Q, "attn_q", "q_proj", 0, 0),
    DESCRIPTOR_RULE("k_projection", YVEX_TENSOR_ROLE_ATTENTION_K, "attn_k", "k_proj", 0, 0),
    DESCRIPTOR_RULE("v_projection", YVEX_TENSOR_ROLE_ATTENTION_V, "attn_v", "v_proj", 0, 0),
    DESCRIPTOR_RULE("o_projection", YVEX_TENSOR_ROLE_ATTENTION_OUT,
                    "attn_output", "o_proj", 0, 0),
    DESCRIPTOR_RULE("mlp_gate", YVEX_TENSOR_ROLE_FFN_GATE, "ffn_gate", "gate_proj", 0, 0),
    DESCRIPTOR_RULE("mlp_up", YVEX_TENSOR_ROLE_FFN_UP, "ffn_up", "up_proj", 0, 0),
    DESCRIPTOR_RULE("mlp_down", YVEX_TENSOR_ROLE_FFN_DOWN, "ffn_down", "down_proj", 0, 0),
    DESCRIPTOR_RULE("moe_router", YVEX_TENSOR_ROLE_MOE_ROUTER, "router", NULL, 0, 0),
    DESCRIPTOR_RULE("moe_expert_gate", YVEX_TENSOR_ROLE_MOE_EXPERT_GATE,
                    "expert", "gate", 1, 0),
    DESCRIPTOR_RULE("moe_expert_up", YVEX_TENSOR_ROLE_MOE_EXPERT_UP,
                    "expert", "up", 1, 0),
    DESCRIPTOR_RULE("moe_expert_down", YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN,
                    "expert", "down", 1, 0),
    DESCRIPTOR_RULE("output_head", YVEX_TENSOR_ROLE_OUTPUT_HEAD,
                    "output.weight", "lm_head", 0, 1),
    DESCRIPTOR_RULE("unknown", YVEX_TENSOR_ROLE_UNKNOWN, NULL, NULL, 0, 0),
};

#undef DESCRIPTOR_RULE

#undef COLLECTION_RULE

/* Match one tensor name rule while preserving its ordered fallback semantics. */
/* Purpose: Compute tensor collection name matches for its CLI invariant (`tensor_collection_name_matches`). */
static int tensor_collection_name_matches(const tensor_collection_rule *rule,
                                          const char *name)
{
    int a = rule->name_a_must_equal ? strcmp(name, rule->name_a) == 0
                                    : fullmodel_name_has(name, rule->name_a);
    int b = rule->name_b && fullmodel_name_has(name, rule->name_b);

    return rule->names_must_both_match ? a && b : a || b;
}

/* Match one logical descriptor role against typed or legacy tensor facts. */
/* Purpose: Compute descriptor tensor rule matches for its CLI invariant (`descriptor_tensor_rule_matches`). */
static int descriptor_tensor_rule_matches(const descriptor_tensor_rule *rule,
                                          const yvex_tensor_info *tensor,
                                          const char *name)
{
    int a;
    int b;

    if (tensor->role == rule->typed_role) return 1;
    if (!rule->name_a) return 0;
    a = rule->name_a_must_equal ? strcmp(name, rule->name_a) == 0
                                : fullmodel_name_has(name, rule->name_a);
    b = rule->name_b && fullmodel_name_has(name, rule->name_b);
    return rule->names_must_both_match ? a && b : a || b;
}

/* Apply one admitted collection rule to typed counters and role presence. */
/* Purpose: Compute tensor collection apply for its CLI invariant (`tensor_collection_apply`). */
static void tensor_collection_apply(const tensor_collection_rule *rule,
                                    const yvex_tensor_info *tensor,
                                    yvex_fullmodel_collections *collections)
{
    unsigned char *base = (unsigned char *)collections;

    fullmodel_collection_add((unsigned long long *)(base + rule->count_offset),
                             (unsigned long long *)(base + rule->bytes_offset), tensor);
    *(int *)(base + rule->flag_offset) = 1;
}

/* Purpose: Compute fullmodel record dtype for its CLI invariant (`fullmodel_record_dtype`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_record_dtype(fullmodel_dtype_bucket buckets[32],
                                   unsigned int *bucket_count,
                                   const yvex_tensor_info *tensor)
{
    const char *name;
    unsigned int i;

    if (!buckets || !bucket_count || !tensor) return;
    name = yvex_dtype_name(tensor->dtype);
    for (i = 0; i < *bucket_count; ++i) {
        if (strcmp(buckets[i].name, name) == 0) {
            buckets[i].count++;
            buckets[i].bytes += tensor->storage_bytes;
            return;
        }
    }
    if (*bucket_count < 32u) {
        snprintf(buckets[*bucket_count].name, sizeof(buckets[*bucket_count].name), "%s", name);
        buckets[*bucket_count].count = 1ull;
        buckets[*bucket_count].bytes = tensor->storage_bytes;
        (*bucket_count)++;
    }
}

/* Purpose: Compute fullmodel dtype summary for its CLI invariant (`fullmodel_dtype_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_dtype_summary(char *out,
                                    size_t out_cap,
                                    const fullmodel_dtype_bucket buckets[32],
                                    unsigned int bucket_count)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    for (i = 0; i < bucket_count; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%s:%llu:%llu",
                         i == 0 ? "" : ",",
                         buckets[i].name,
                         buckets[i].count,
                         buckets[i].bytes);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (bucket_count == 0u) snprintf(out, out_cap, "none");
}

/* Purpose: Compute fullmodel record largest for its CLI invariant (`fullmodel_record_largest`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_record_largest(fullmodel_largest_tensor top[16],
                                     unsigned int *top_count,
                                     unsigned int limit,
                                     const yvex_tensor_info *tensor)
{
    unsigned int i;
    unsigned int pos;

    if (!top || !top_count || !tensor || limit == 0u) return;
    if (limit > 16u) limit = 16u;
    pos = *top_count;
    for (i = 0; i < *top_count; ++i) {
        if (tensor->storage_bytes > top[i].bytes) {
            pos = i;
            break;
        }
    }
    if (*top_count < limit) {
        (*top_count)++;
    } else if (pos >= limit) {
        return;
    }
    for (i = *top_count - 1u; i > pos; --i) {
        top[i] = top[i - 1u];
    }
    top[pos].tensor = tensor;
    top[pos].bytes = tensor->storage_bytes;
}

/* Purpose: Compute fullmodel classify tensor for its CLI invariant (`fullmodel_classify_tensor`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_classify_tensor(const yvex_tensor_info *tensor,
                                      yvex_fullmodel_collections *collections)
{
    const char *name;
    size_t i;

    if (!tensor || !collections) return;
    name = tensor->name ? tensor->name : "";
    for (i = 0; i < sizeof(typed_collection_rules) / sizeof(typed_collection_rules[0]); ++i) {
        if (typed_collection_rules[i].role == tensor->role) {
            tensor_collection_apply(&typed_collection_rules[i], tensor, collections);
            return;
        }
    }
    for (i = 0; i < sizeof(named_collection_rules) / sizeof(named_collection_rules[0]); ++i) {
        if (tensor_collection_name_matches(&named_collection_rules[i], name)) {
            tensor_collection_apply(&named_collection_rules[i], tensor, collections);
            return;
        }
    }
    fullmodel_collection_add(&collections->unknown, &collections->unknown_bytes, tensor);
}

/* Purpose: Compute fullmodel is selected target for its CLI invariant (`fullmodel_is_selected_target`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_is_selected_target(const char *text)
{
    return text &&
           (strcmp(text, "deepseek4-v4-flash-selected-embed") == 0 ||
            strcmp(text, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0);
}

/* Purpose: Compute fullmodel descriptor tensor matches for its CLI invariant
 *   (`fullmodel_descriptor_tensor_matches`). */
static int fullmodel_descriptor_tensor_matches(const yvex_tensor_info *tensor,
                                               const char *role)
{
    const char *name;
    size_t i;

    if (!tensor || !role) return 0;
    name = tensor->name ? tensor->name : "";
    for (i = 0; i < sizeof(descriptor_tensor_rules) / sizeof(descriptor_tensor_rules[0]); ++i) {
        if (strcmp(role, descriptor_tensor_rules[i].logical_role) == 0) {
            return descriptor_tensor_rule_matches(&descriptor_tensor_rules[i], tensor, name);
        }
    }
    return 0;
}

/* Purpose: Compute fullmodel descriptor find tensor for its CLI invariant (`fullmodel_descriptor_find_tensor`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const yvex_tensor_info *fullmodel_descriptor_find_tensor(yvex_model_context *ctx,
                                                                const char *role)
{
    unsigned long long count;
    unsigned long long i;

    if (!ctx || !ctx->table || !role) return NULL;
    count = yvex_tensor_table_count(ctx->table);
    for (i = 0; i < count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        if (fullmodel_descriptor_tensor_matches(tensor, role)) return tensor;
    }
    return NULL;
}

/* Purpose: Compute fullmodel detect family for its CLI invariant (`fullmodel_detect_family`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *fullmodel_detect_family(const yvex_cli_fullmodel_options *options,
                                           yvex_arch arch,
                                           const char *target_id)
{
    const char *from_arch = fullmodel_family_from_arch(arch);

    if (from_arch && strcmp(from_arch, "unknown") != 0) return from_arch;
    if (target_id && fullmodel_name_has(target_id, "deepseek")) return "deepseek";
    if (options && options->model && fullmodel_name_has(options->model, "deepseek")) return "deepseek";
    if (target_id && fullmodel_name_has(target_id, "glm")) return "glm";
    if (options && options->model && fullmodel_name_has(options->model, "glm")) return "glm";
    if (target_id && fullmodel_name_has(target_id, "qwen")) return "qwen";
    if (options && options->model && fullmodel_name_has(options->model, "qwen")) return "qwen";
    return "unknown";
}

/* Purpose: Compute fullmodel family request matches for its CLI invariant (`fullmodel_family_request_matches`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_family_request_matches(const char *requested,
                                            const char *detected)
{
    if (!requested || !requested[0] || strcmp(requested, "auto") == 0) {
        return detected && strcmp(detected, "unknown") != 0;
    }
    return detected && strcmp(requested, detected) == 0;
}

/* Purpose: Compute fullmodel role status from tensor for its CLI invariant (`fullmodel_role_status_from_tensor`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *fullmodel_role_status_from_tensor(yvex_model_context *ctx,
                                                     const yvex_fullmodel_collections *collections,
                                                     const char *role)
{
    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        return collections && collections->has_tokenizer_metadata ? "present" : "missing";
    }
    return fullmodel_descriptor_find_tensor(ctx, role) ? "present" : "missing";
}

/* Purpose: Compute fullmodel identity status for its CLI invariant (`fullmodel_identity_status`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *fullmodel_identity_status(const yvex_model_ref *ref,
                                             unsigned long long artifact_bytes)
{
    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) return "not-checked";
    if (!ref->sha256 || !ref->sha256[0]) return "registered-without-digest";
    if (ref->registered_file_size != 0ull && ref->registered_file_size != artifact_bytes) {
        return "registered-size-drift";
    }
    return "registered-size-match";
}

/* Purpose: Compute fullmodel probe backend fit for its CLI invariant (`fullmodel_probe_backend_fit`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_probe_backend_fit(const char *backend,
                                        unsigned long long required_bytes,
                                        yvex_fullmodel_backend_fit *fit)
{
    yvex_backend *opened = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    yvex_error err;
    int rc;

    if (!fit) return;
    memset(fit, 0, sizeof(*fit));
    fit->required_bytes = required_bytes;
    fit->fit_status = "unknown";
    snprintf(fit->fit_reason, sizeof(fit->fit_reason),
             "system memory availability is not queried");

    if (!backend || strcmp(backend, "cpu") == 0) {
        fit->available = 1;
        return;
    }

    memset(&options, 0, sizeof(options));
    memset(&stats, 0, sizeof(stats));
    memset(&device_info, 0, sizeof(device_info));
    yvex_error_clear(&err);
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&opened, &options, &err);
    if (rc != YVEX_OK) {
        fit->available = 0;
        fit->fit_status = "unsupported";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA backend unavailable: %s", yvex_error_message(&err));
        yvex_error_clear(&err);
        return;
    }

    fit->available = 1;
    rc = yvex_backend_get_memory_stats(opened, &stats, &err);
    if (rc == YVEX_OK &&
        yvex_backend_get_device_info(opened, &device_info, &err) == YVEX_OK &&
        device_info.total_memory_bytes > 0ull) {
        fit->memory_known = 1;
        fit->total_bytes = device_info.total_memory_bytes;
        fit->available_bytes = device_info.free_memory_bytes;
        if (required_bytes <= device_info.free_memory_bytes) {
            fit->fit_status = "fits";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes fit current CUDA free memory; no allocation attempted");
        } else {
            fit->fit_status = "does-not-fit";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes exceed current CUDA free memory; no allocation attempted");
        }
    } else {
        fit->fit_status = "unknown";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA memory info unavailable; no allocation attempted");
        yvex_error_clear(&err);
    }
    yvex_backend_close(opened);
}

/* Purpose: Compute fullmodel has attention collection for its CLI invariant (`fullmodel_has_attention_collection`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_has_attention_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_attention_q &&
           collections->has_attention_k &&
           collections->has_attention_v &&
           collections->has_attention_out;
}

/* Purpose: Compute fullmodel has mlp collection for its CLI invariant (`fullmodel_has_mlp_collection`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_has_mlp_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_ffn_gate &&
           collections->has_ffn_up &&
           collections->has_ffn_down;
}

/* Purpose: Compute fullmodel has normalization collection for its CLI invariant
 * (`fullmodel_has_normalization_collection`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_has_normalization_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           (collections->has_attention_norm ||
            collections->has_post_attention_norm ||
            collections->has_output_norm);
}

/* Purpose: Compute fullmodel tensor is materialize required for its CLI invariant
 * (`fullmodel_tensor_is_materialize_required`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_tensor_is_materialize_required(const yvex_tensor_info *tensor)
{
    const char *name;

    if (!tensor) return 0;
    name = tensor->name ? tensor->name : "";
    switch (tensor->role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
    case YVEX_TENSOR_ROLE_ATTENTION_NORM:
    case YVEX_TENSOR_ROLE_ATTENTION_Q:
    case YVEX_TENSOR_ROLE_ATTENTION_K:
    case YVEX_TENSOR_ROLE_ATTENTION_V:
    case YVEX_TENSOR_ROLE_ATTENTION_OUT:
    case YVEX_TENSOR_ROLE_FFN_NORM:
    case YVEX_TENSOR_ROLE_FFN_GATE:
    case YVEX_TENSOR_ROLE_FFN_UP:
    case YVEX_TENSOR_ROLE_FFN_DOWN:
        return 1;
    default:
        break;
    }
    return fullmodel_name_has(name, "token_embd") ||
           fullmodel_name_has(name, "attn_norm") ||
           fullmodel_name_has(name, "ffn_norm") ||
           fullmodel_name_has(name, "attn_q") ||
           fullmodel_name_has(name, "attn_k") ||
           fullmodel_name_has(name, "attn_v") ||
           fullmodel_name_has(name, "attn_output") ||
           fullmodel_name_has(name, "q_proj") ||
           fullmodel_name_has(name, "k_proj") ||
           fullmodel_name_has(name, "v_proj") ||
           fullmodel_name_has(name, "o_proj") ||
           fullmodel_name_has(name, "ffn_gate") ||
           fullmodel_name_has(name, "ffn_up") ||
           fullmodel_name_has(name, "ffn_down") ||
           fullmodel_name_has(name, "gate_proj") ||
           fullmodel_name_has(name, "up_proj") ||
           fullmodel_name_has(name, "down_proj") ||
           fullmodel_name_has(name, "output_norm") ||
           strcmp(name, "output.weight") == 0 ||
           fullmodel_name_has(name, "lm_head");
}

/* Purpose: Compute fullmodel role present for its CLI invariant (`fullmodel_role_present`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_role_present(const yvex_fullmodel_collections *collections,
                                  const char *role)
{
    if (!collections || !role) return 0;
    if (strcmp(role, "token-embedding") == 0) return collections->has_token_embedding;
    if (strcmp(role, "attention-norm") == 0) return collections->has_attention_norm;
    if (strcmp(role, "post-attention-norm") == 0) return collections->has_post_attention_norm;
    if (strcmp(role, "attention-q-projection") == 0) return collections->has_attention_q;
    if (strcmp(role, "attention-k-projection") == 0) return collections->has_attention_k;
    if (strcmp(role, "attention-v-projection") == 0) return collections->has_attention_v;
    if (strcmp(role, "attention-output-projection") == 0) return collections->has_attention_out;
    if (strcmp(role, "mlp-gate") == 0) return collections->has_ffn_gate;
    if (strcmp(role, "mlp-up") == 0) return collections->has_ffn_up;
    if (strcmp(role, "mlp-down") == 0) return collections->has_ffn_down;
    if (strcmp(role, "moe-router") == 0) return collections->has_moe_router;
    if (strcmp(role, "moe-experts") == 0) return collections->has_moe_expert;
    if (strcmp(role, "final-norm") == 0) return collections->has_output_norm;
    if (strcmp(role, "output-head") == 0) return collections->has_output_head;
    if (strcmp(role, "tokenizer-metadata") == 0) return collections->has_tokenizer_metadata;
    return 0;
}

/* Purpose: Compute fullmodel collection present by name for its CLI invariant
 *   (`fullmodel_collection_present_by_name`). */
static int fullmodel_collection_present_by_name(const yvex_fullmodel_collections *collections,
                                                const char *collection)
{
    if (!collections || !collection) return 0;
    if (strcmp(collection, "embedding") == 0) return collections->embedding > 0ull;
    if (strcmp(collection, "normalization") == 0) return collections->normalization > 0ull;
    if (strcmp(collection, "attention") == 0) return fullmodel_has_attention_collection(collections);
    if (strcmp(collection, "mlp") == 0) return fullmodel_has_mlp_collection(collections);
    if (strcmp(collection, "moe") == 0) return collections->moe > 0ull;
    if (strcmp(collection, "output") == 0) return collections->output > 0ull;
    if (strcmp(collection, "tokenizer") == 0 ||
        strcmp(collection, "tokenizer-runtime-input") == 0) {
        return collections->has_tokenizer_metadata;
    }
    return 0;
}

/* Purpose: Compute fullmodel materialize missing roles for its CLI invariant (`fullmodel_materialize_missing_roles`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_materialize_missing_roles(const yvex_cli_fullmodel_options *options,
                                                const yvex_fullmodel_collections *collections,
                                                char *out,
                                                size_t out_cap)
{
    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (!fullmodel_role_present(collections, "token-embedding")) fullmodel_csv_append(out, out_cap, "token-embedding");
    if (!fullmodel_role_present(collections, "attention-norm")) fullmodel_csv_append(out, out_cap, "attention-norm");
    if (!fullmodel_role_present(collections, "post-attention-norm")) fullmodel_csv_append(out, out_cap,
        "post-attention-norm");
    if (!fullmodel_role_present(collections, "attention-q-projection")) fullmodel_csv_append(out, out_cap,
        "attention-q-projection");
    if (!fullmodel_role_present(collections, "attention-k-projection")) fullmodel_csv_append(out, out_cap,
        "attention-k-projection");
    if (!fullmodel_role_present(collections, "attention-v-projection")) fullmodel_csv_append(out, out_cap,
        "attention-v-projection");
    if (!fullmodel_role_present(collections, "attention-output-projection")) fullmodel_csv_append(out,
        out_cap, "attention-output-projection");
    if (!fullmodel_role_present(collections, "mlp-gate")) fullmodel_csv_append(out, out_cap, "mlp-gate");
    if (!fullmodel_role_present(collections, "mlp-up")) fullmodel_csv_append(out, out_cap, "mlp-up");
    if (!fullmodel_role_present(collections, "mlp-down")) fullmodel_csv_append(out, out_cap, "mlp-down");
    if (!fullmodel_role_present(collections, "final-norm")) fullmodel_csv_append(out, out_cap, "final-norm");
    if (!fullmodel_role_present(collections, "output-head")) fullmodel_csv_append(out, out_cap, "output-head");
    if (!fullmodel_role_present(collections, "tokenizer-metadata")) fullmodel_csv_append(out, out_cap,
        "tokenizer-metadata");
    if (options && options->require_role &&
        !fullmodel_role_present(collections, options->require_role)) {
        fullmodel_csv_append(out, out_cap, options->require_role);
    }
    if (options && options->require_collection &&
        !fullmodel_collection_present_by_name(collections, options->require_collection)) {
        char tmp[160];
        snprintf(tmp, sizeof(tmp), "collection:%s", options->require_collection);
        fullmodel_csv_append(out, out_cap, tmp);
    }
    if (!out[0]) snprintf(out, out_cap, "none");
}

/* Purpose: Compute fullmodel fail after for its CLI invariant (`fullmodel_fail_after`). */
static int fullmodel_fail_after(const yvex_cli_fullmodel_options *options,
                                const char *phase)
{
    return options && options->fail_after_phase && phase &&
           strcmp(options->fail_after_phase, phase) == 0;
}

/* Purpose: Construct the owned fullmodel open requested backend state (`fullmodel_open_requested_backend`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_open_requested_backend(const char *backend_name,
                                            yvex_backend **out,
                                            yvex_error *err)
{
    yvex_backend_options options;

    if (!out) return YVEX_ERR_INVALID_ARG;
    *out = NULL;
    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        return yvex_backend_open_cpu(out, err);
    }
    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    return yvex_backend_open(out, &options, err);
}

/* Purpose: Construct the owned fullmodel allocate required tensors state (`fullmodel_allocate_required_tensors`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_allocate_required_tensors(const yvex_cli_fullmodel_options *options,
                                               yvex_model_context *ctx,
                                               unsigned long long *materialized_count,
                                               unsigned long long *materialized_bytes,
                                               const char **failed_phase,
                                               const char **failed_reason)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor **allocated = NULL;
    yvex_error err;
    unsigned long long tensor_count;
    unsigned long long i;
    unsigned long long allocated_count = 0ull;
    int rc;

    if (materialized_count) *materialized_count = 0ull;
    if (materialized_bytes) *materialized_bytes = 0ull;
    if (failed_phase) *failed_phase = "none";
    if (failed_reason) *failed_reason = "none";
    if (!options || !ctx || !ctx->table) return YVEX_ERR_INVALID_ARG;

    tensor_count = yvex_tensor_table_count(ctx->table);
    allocated = (yvex_device_tensor **)calloc((size_t)(tensor_count ? tensor_count : 1ull),
                                              sizeof(*allocated));
    if (!allocated) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "allocation-list";
        return YVEX_ERR_NOMEM;
    }

    yvex_error_clear(&err);
    rc = fullmodel_open_requested_backend(options->backend, &backend, &err);
    if (rc != YVEX_OK) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "backend-open-failed";
        yvex_error_clear(&err);
        free(allocated);
        return rc;
    }

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        yvex_backend_tensor_desc desc;
        yvex_device_tensor *device_tensor = NULL;
        unsigned int d;

        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        memset(&desc, 0, sizeof(desc));
        desc.name = tensor->name;
        desc.dtype = tensor->dtype;
        desc.rank = tensor->rank;
        desc.bytes = tensor->storage_bytes;
        for (d = 0; d < tensor->rank && d < YVEX_TENSOR_MAX_DIMS; ++d) {
            desc.dims[d] = tensor->dims[d];
        }
        rc = yvex_backend_tensor_alloc(backend, &desc, &device_tensor, &err);
        if (rc != YVEX_OK) {
            if (failed_phase) *failed_phase = "backend-preflight";
            if (failed_reason) *failed_reason = "tensor-allocation-failed";
            yvex_error_clear(&err);
            break;
        }
        allocated[allocated_count++] = device_tensor;
        if (materialized_count) (*materialized_count)++;
        if (materialized_bytes) *materialized_bytes += tensor->storage_bytes;
    }

    while (allocated_count > 0ull) {
        allocated_count--;
        yvex_backend_tensor_free(backend, allocated[allocated_count]);
    }
    yvex_backend_close(backend);
    free(allocated);
    return rc;
}

/* Seed the transactional materialization report before any faultable phase. */
/* Purpose: Construct the owned fullmodel materialize report init state (`fullmodel_materialize_report_init`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_materialize_report_init(fullmodel_materialize_report *report,
                                              const yvex_cli_fullmodel_options *options,
                                              const yvex_model_ref *ref,
                                              const char *target_id,
                                              const char *target_class,
                                              unsigned long long artifact_bytes,
                                              unsigned long long required_count,
                                              unsigned long long required_bytes,
                                              const char *missing_roles,
                                              int selected_target,
                                              int role_complete)
{
    static const char *unsupported =
        "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,real-DeepSeek-decode,"
            "real-output-head-logits,real-vocabulary-sampling";

    report->options = options;
    report->status = "fullmodel-materialize-fail";
    report->model_resolved_path = ref && ref->path ? ref->path : "";
    report->target_id = target_id;
    report->target_class = target_class;
    report->artifact_identity_status = fullmodel_identity_status(ref, artifact_bytes);
    report->tensor_inventory_status = "pass";
    report->required_role_coverage = selected_target ? "partial" :
                                     role_complete ? "complete" : "partial";
    report->missing_required_roles = missing_roles;
    report->unsupported_required_roles = unsupported;
    report->placement_plan_status = "pass";
    report->memory_budget_status = "pass";
    report->backend_preflight_status = "pass";
    report->materialization_mode = "controlled-fullmodel-proof";
    report->full_model_materialization = "controlled-tiny-proof";
    report->full_model_materialization_proof = "fail";
    report->phase = "failed";
    report->failed_phase = "none";
    report->failed_reason = "none";
    report->cleanup_attempted = "true";
    report->cleanup_status = "pass";
    report->cleanup_idempotent = "true";
    report->owned_state_released = "true";
    report->partial_materialization = "false";
    report->required_tensor_count = required_count;
    report->required_tensor_bytes = required_bytes;
    report->peak_planned_bytes = required_bytes;
    report->cpu_resident_bytes = strcmp(options->backend, "cuda") == 0 ? 0ull : required_bytes;
    report->cuda_resident_bytes = strcmp(options->backend, "cuda") == 0 ? required_bytes : 0ull;
    report->residency_plan = strcmp(options->backend, "cuda") == 0
                                 ? "cuda-resident-controlled-proof"
                                 : "cpu-resident-controlled-proof";
    report->runtime_blockers =
        "runtime family adapter not implemented; real transformer prefill unsupported; decode/logits/"
            "sampling/generation remain unsupported-full-model";
}

/* Apply the fault seam for phases completed before role admission. */
/* Purpose: Construct the owned fullmodel materialize initial fault state (`fullmodel_materialize_initial_fault`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_materialize_initial_fault(const yvex_cli_fullmodel_options *options,
                                               fullmodel_materialize_report *report)
{
    static const char *phases[] = {
        "preflight", "resolve-model", "artifact-identity", "tensor-inventory"
    };
    size_t i;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        if (!fullmodel_fail_after(options, phases[i])) continue;
        report->failed_phase = phases[i];
        report->failed_reason = "injected-failure";
        fullmodel_print_materialize_report(report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    return 0;
}

/* Purpose: Orchestrate the typed fullmodel materialize command run request (`fullmodel_materialize_command_run`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_materialize_command_run(const yvex_cli_fullmodel_options *options,
                                             yvex_model_ref *ref,
                                             yvex_model_context *ctx,
                                             const char *target_id,
                                             const char *target_class,
                                             unsigned long long artifact_bytes,
                                             unsigned long long tensor_count,
                                             unsigned long long total_tensor_bytes,
                                             const yvex_fullmodel_collections *collections,
                                             int selected_target)
{
    static const unsigned long long proof_byte_limit = 64ull * 1024ull * 1024ull;
    fullmodel_materialize_report report;
    char materialize_missing_roles[768];
    unsigned long long required_tensor_count = 0ull;
    unsigned long long required_tensor_bytes = 0ull;
    unsigned long long materialized_count = 0ull;
    unsigned long long materialized_bytes = 0ull;
    unsigned long long i;
    const char *alloc_failed_phase = "none";
    const char *alloc_failed_reason = "none";
    int role_complete;
    int rc;

    (void)total_tensor_bytes;
    memset(&report, 0, sizeof(report));
    memset(materialize_missing_roles, 0, sizeof(materialize_missing_roles));
    fullmodel_materialize_missing_roles(options,
                                        collections,
                                        materialize_missing_roles,
                                        sizeof(materialize_missing_roles));
    role_complete = strcmp(materialize_missing_roles, "none") == 0;

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        required_tensor_count++;
        required_tensor_bytes += tensor->storage_bytes;
    }

    fullmodel_materialize_report_init(&report, options, ref, target_id, target_class,
                                      artifact_bytes, required_tensor_count,
                                      required_tensor_bytes, materialize_missing_roles,
                                      selected_target, role_complete);
    rc = fullmodel_materialize_initial_fault(options, &report);
    if (rc != 0) return rc;

    if (selected_target) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "refused";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "selected-runtime-slice-refusal";
        report.full_model_materialization = "refused-selected-runtime-slice";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "selected-runtime-slice";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "selected-slice-not-full-model";
        report.runtime_blockers = "selected runtime slice cannot satisfy full required tensor materialization";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (!role_complete) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "partial";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "role-coverage-refusal";
        report.full_model_materialization = "refused-incomplete-role-coverage";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "required-role-missing";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "not-planned";
        report.runtime_blockers = "required fullmodel proof roles are missing";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "role-coverage")) {
        report.failed_phase = "role-coverage";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "placement-plan")) {
        report.failed_phase = "placement-plan";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if ((options->has_limit_bytes && required_tensor_bytes > options->limit_bytes) ||
        (!options->has_limit_bytes && required_tensor_bytes > proof_byte_limit)) {
        report.status = "fullmodel-materialize-fail";
        report.memory_budget_status = "fail";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "memory-budget-refusal";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = "memory-budget";
        report.failed_reason = options->has_limit_bytes ? "byte-limit" : "controlled-proof-limit";
        report.refused_tensor_count = required_tensor_count;
        report.skipped_tensor_count = required_tensor_count;
        report.residency_plan = "not-planned";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_NOMEM);
    }

    if (fullmodel_fail_after(options, "memory-budget")) {
        report.failed_phase = "memory-budget";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if (options->dry_run || options->plan_only) {
        report.status = options->plan_only ? "fullmodel-materialize-plan-only" : "fullmodel-materialize-dry-run";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = options->plan_only ? "plan-only" : "dry-run";
        report.full_model_materialization = "planned";
        report.full_model_materialization_proof = "planned";
        report.phase = options->plan_only ? "placement-plan" : "memory-budget";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.skipped_tensor_count = required_tensor_count;
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "backend-preflight")) {
        report.failed_phase = "backend-preflight";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    rc = fullmodel_allocate_required_tensors(options,
                                             ctx,
                                             &materialized_count,
                                             &materialized_bytes,
                                             &alloc_failed_phase,
                                             &alloc_failed_reason);
    if (rc != YVEX_OK) {
        report.status = "fullmodel-materialize-fail";
        report.backend_preflight_status = strcmp(alloc_failed_phase, "backend-preflight") == 0 ? "fail" : "partial";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = alloc_failed_phase ? alloc_failed_phase : "backend-preflight";
        report.failed_reason = alloc_failed_reason ? alloc_failed_reason : "allocation failed";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = materialized_count > 0ull ? "true" : "false";
        report.refused_tensor_count = required_tensor_count > materialized_count
                                          ? required_tensor_count - materialized_count
                                          : 0ull;
        fullmodel_print_materialize_report(&report);
        return exit_for_status(rc);
    }

    if (fullmodel_fail_after(options, "materialize-embedding") ||
        fullmodel_fail_after(options, "materialize-normalization") ||
        fullmodel_fail_after(options, "materialize-attention") ||
        fullmodel_fail_after(options, "materialize-mlp") ||
        fullmodel_fail_after(options, "materialize-moe") ||
        fullmodel_fail_after(options, "materialize-output") ||
        fullmodel_fail_after(options, "materialize-tokenizer") ||
        fullmodel_fail_after(options, "cleanup")) {
        report.failed_phase = options->fail_after_phase;
        report.failed_reason = "injected-failure";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = "false";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    report.status = "fullmodel-materialize-pass";
    report.full_model_materialization_proof = "pass";
    report.phase = "complete";
    report.materialized_tensor_count = materialized_count;
    report.materialized_tensor_bytes = materialized_bytes;
    report.refused_tensor_count = 0ull;
    report.skipped_tensor_count = tensor_count > materialized_count
                                      ? tensor_count - materialized_count
                                      : 0ull;
    fullmodel_print_materialize_report(&report);
    return 0;
}

typedef struct {
    yvex_cli_fullmodel_options *options;
    yvex_model_ref *ref;
    yvex_model_context *ctx;
    yvex_fullmodel_collections *collections;
    fullmodel_largest_tensor *largest;
    const char *target_id;
    const char *target_class;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *model_class;
    const char *inventory_status;
    const char *role_coverage;
    const char *backend_placement_status;
    const char *cpu_placement;
    const char *cuda_placement;
    const char *dtype_summary;
    const char *missing_roles;
    const char *descriptor_missing_roles;
    const char *unsupported_roles;
    yvex_arch arch;
    unsigned long long artifact_bytes;
    unsigned long long tensor_count;
    unsigned long long total_tensor_bytes;
    unsigned int largest_count;
    int selected_target;
} fullmodel_surface_view;

typedef struct {
    yvex_model_ref ref;
    yvex_model_context ctx;
    yvex_fullmodel_collections collections;
    fullmodel_dtype_bucket dtype_buckets[32];
    fullmodel_largest_tensor largest[16];
    char dtype_summary[512];
    char missing_roles[768];
    char descriptor_missing_roles[768];
    char unsupported_roles[512];
    fullmodel_surface_view view;
} fullmodel_surface_state;

typedef struct missing_role_spec {
    size_t present_offset;
    const char *name;
} missing_role_spec;

#define MISSING_ROLE(member_, name_) \
    {offsetof(yvex_fullmodel_collections, member_), name_}

static const missing_role_spec missing_role_specs[] = {
    MISSING_ROLE(has_token_embedding, "token-embedding"),
    MISSING_ROLE(has_attention_norm, "attention-norm"),
    MISSING_ROLE(has_post_attention_norm, "post-attention-norm"),
    MISSING_ROLE(has_attention_q, "attention-q-projection"),
    MISSING_ROLE(has_attention_k, "attention-k-projection"),
    MISSING_ROLE(has_attention_v, "attention-v-projection"),
    MISSING_ROLE(has_attention_out, "attention-output-projection"),
    MISSING_ROLE(has_ffn_gate, "mlp-gate"),
    MISSING_ROLE(has_ffn_up, "mlp-up"),
    MISSING_ROLE(has_ffn_down, "mlp-down"),
    MISSING_ROLE(has_moe_router, "moe-router"),
    MISSING_ROLE(has_moe_expert, "moe-experts"),
    MISSING_ROLE(has_output_norm, "final-norm"),
    MISSING_ROLE(has_output_head, "output-head"),
    MISSING_ROLE(has_tokenizer_metadata, "tokenizer-metadata"),
};

static const char *const unsupported_runtime_roles[] = {
    "runtime-family-adapter", "real-transformer-prefill", "real-DeepSeek-decode",
    "real-output-head-logits", "real-vocabulary-sampling"};

#undef MISSING_ROLE

/* Handle the admitted source-only target without opening a GGUF model context. */
/* Purpose: Compute fullmodel source only dispatch for its CLI invariant (`fullmodel_source_only_dispatch`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_source_only_dispatch(const yvex_cli_fullmodel_options *options,
                                          int *handled)
{
    const char *target = "glm-5.2-official-safetensors";

    *handled = strcmp(options->model, target) == 0 ||
               (options->target && strcmp(options->target, target) == 0);
    if (!*handled) return 0;
    if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
        return print_fullmodel_source_only_plan(options, target);
    }
    if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
        return print_fullmodel_source_only_materialize(options, target);
    }
    if (options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
        return print_fullmodel_source_only_descriptor(options, target);
    }
    if (options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
        return print_fullmodel_source_only_family_runtime(options, target);
    }
    return print_fullmodel_source_only_report(target, options->backend);
}

/* Collect tensor directory facts once for every downstream fullmodel report mode. */
/* Purpose: Compute fullmodel surface inventory for its CLI invariant (`fullmodel_surface_inventory`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_surface_inventory(fullmodel_surface_state *state,
                                        const yvex_cli_fullmodel_options *options)
{
    unsigned long long i;
    unsigned int bucket_count = 0u;
    unsigned int largest_count = 0u;

    state->view.tensor_count = yvex_tensor_table_count(state->ctx.table);
    for (i = 0; i < state->view.tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(state->ctx.table, i);
        if (!tensor) continue;
        state->view.total_tensor_bytes += tensor->storage_bytes;
        fullmodel_record_dtype(state->dtype_buckets, &bucket_count, tensor);
        fullmodel_record_largest(state->largest, &largest_count,
                                 (unsigned int)options->limit_tensors, tensor);
        fullmodel_classify_tensor(tensor, &state->collections);
    }
    if (yvex_gguf_metadata_find(state->ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(state->ctx.gguf, "tokenizer.ggml.model")) {
        state->collections.tokenizer = 1ull;
        state->collections.tokenizer_bytes = 0ull;
        state->collections.has_tokenizer_metadata = 1;
    }
    state->view.largest_count = largest_count;
    fullmodel_dtype_summary(state->dtype_summary, sizeof(state->dtype_summary),
                            state->dtype_buckets, bucket_count);
}

/* Derive role coverage and explicit unsupported runtime boundaries from inventory facts. */
/* Purpose: Compute fullmodel surface roles for its CLI invariant (`fullmodel_surface_roles`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_surface_roles(fullmodel_surface_state *state,
                                    const yvex_cli_fullmodel_options *options)
{
    yvex_fullmodel_collections *c = &state->collections;
    size_t i;

    for (i = 0; i < sizeof(missing_role_specs) / sizeof(missing_role_specs[0]); ++i) {
        const int *present = (const int *)((const unsigned char *)c +
                                          missing_role_specs[i].present_offset);
        if (!*present) fullmodel_csv_append(state->missing_roles,
                                            sizeof(state->missing_roles),
                                            missing_role_specs[i].name);
    }
    if (!state->missing_roles[0]) {
        snprintf(state->missing_roles, sizeof(state->missing_roles), "none");
    }
    for (i = 0; i < sizeof(unsupported_runtime_roles) / sizeof(unsupported_runtime_roles[0]); ++i) {
        fullmodel_csv_append(state->unsupported_roles, sizeof(state->unsupported_roles),
                             unsupported_runtime_roles[i]);
    }
    fullmodel_materialize_missing_roles(options, c, state->descriptor_missing_roles,
                                        sizeof(state->descriptor_missing_roles));
}

/* Resolve the artifact, retain one model context, and build the immutable command view. */
/* Purpose: Construct the owned fullmodel surface prepare state (`fullmodel_surface_prepare`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_surface_prepare(yvex_cli_fullmodel_options *options,
                                     fullmodel_surface_state *state)
{
    yvex_model_ref_options ref_options;
    yvex_error err;
    fullmodel_surface_view *view = &state->view;
    int rc;

    memset(state, 0, sizeof(*state));
    memset(&ref_options, 0, sizeof(ref_options));
    yvex_error_clear(&err);
    ref_options.allow_registry = 1;
    ref_options.registry_path = options->registry_path;
    rc = yvex_model_ref_resolve(&state->ref, options->model, &ref_options, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (!fullmodel_file_size(state->ref.path, &view->artifact_bytes)) {
        rc = print_fullmodel_missing_report(options, state->ref.path);
        yvex_model_ref_clear(&state->ref);
        return rc;
    }
    rc = yvex_model_context_open(state->ref.path, &state->ctx, &err);
    if (rc != YVEX_OK) {
        int out = print_fullmodel_parse_failure_report(options, &state->ref,
                                                       yvex_error_message(&err), rc);
        yvex_error_clear(&err);
        yvex_model_ref_clear(&state->ref);
        return out;
    }
    view->options = options;
    view->ref = &state->ref;
    view->ctx = &state->ctx;
    view->collections = &state->collections;
    view->largest = state->largest;
    fullmodel_surface_inventory(state, options);
    view->arch = yvex_model_arch(state->ctx.model);
    view->target_id = options->target ? options->target :
        (state->ref.alias && state->ref.alias[0] ? state->ref.alias : "path");
    view->selected_target = fullmodel_is_selected_target(view->target_id) ||
                            fullmodel_is_selected_target(options->model);
    view->target_class = view->selected_target ? "selected-runtime-slice" :
        (options->target && strcmp(options->target, "deepseek4-v4-flash") == 0
             ? "full-runtime-model-planned" : "candidate-GGUF-path");
    view->source_artifact_class = view->selected_target
        ? "YVEX-produced selected GGUF" : "GGUF artifact";
    view->target_artifact_class = view->selected_target
        ? "YVEX-produced selected GGUF" : "candidate GGUF artifact";
    view->model_class = view->selected_target
        ? "selected-runtime-slice" : "descriptor-only-candidate";
    view->inventory_status = view->selected_target ? "incomplete" : "partial";
    fullmodel_surface_roles(state, options);
    view->role_coverage = strcmp(state->missing_roles, "none") == 0 ? "observed" : "partial";
    if (view->selected_target) view->role_coverage = "partial";
    view->backend_placement_status = view->selected_target
        ? "selected-tensor-plan-only" : "report-only";
    view->cpu_placement = view->selected_target
        ? "selected-tensors-only" : "planned-full-model";
    view->cuda_placement = strcmp(options->backend, "cuda") == 0
        ? (yvex_backend_cuda_context_available()
               ? "selected-or-candidate-tensors-only" : "unavailable")
        : "not-requested";
    view->dtype_summary = state->dtype_summary;
    view->missing_roles = state->missing_roles;
    view->descriptor_missing_roles = state->descriptor_missing_roles;
    view->unsupported_roles = state->unsupported_roles;
    return YVEX_OK;
}

/* Dispatch non-report commands and compact report mode from one admitted view. */
/* Purpose: Compute fullmodel surface dispatch for its CLI invariant (`fullmodel_surface_dispatch`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_surface_dispatch(fullmodel_surface_view *view, int *handled)
{
    yvex_cli_fullmodel_options *options = view->options;
    const char *descriptor_coverage = view->selected_target ? "partial" :
        strcmp(view->descriptor_missing_roles, "none") == 0 ? "complete" : "partial";
    int rc = 0;

    *handled = 1;
    if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
        rc = fullmodel_materialize_command_run(options, view->ref, view->ctx,
            view->target_id, view->target_class, view->artifact_bytes,
            view->tensor_count, view->total_tensor_bytes, view->collections,
            view->selected_target);
    } else if (options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
        if (options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_descriptor_normal(options, view->target_id,
                view->target_class, descriptor_coverage, view->descriptor_missing_roles);
        } else {
            fullmodel_print_descriptor_report(options, view->ref, view->ctx,
                view->target_id, view->target_class, view->artifact_bytes, view->arch,
                view->tensor_count, view->total_tensor_bytes, view->collections,
                descriptor_coverage, view->descriptor_missing_roles,
                view->unsupported_roles, view->selected_target);
        }
    } else if (options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
        if (options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_family_runtime_normal(options, view->target_id,
                view->target_class, descriptor_coverage, view->descriptor_missing_roles);
        } else {
            rc = fullmodel_print_family_runtime_report(options, view->ref, view->ctx,
                view->target_id, view->target_class, view->artifact_bytes, view->arch,
                view->tensor_count, view->total_tensor_bytes, view->collections,
                descriptor_coverage, view->descriptor_missing_roles,
                view->unsupported_roles, view->selected_target);
        }
    } else if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
        if (options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_plan_normal(options,
                view->selected_target ? "blocked" : "partial", view->target_class,
                view->selected_target ? "blocked" : "unknown",
                view->selected_target ? "selected-slice-not-full-runtime" :
                                        "full-runtime candidate artifact required");
        } else {
            fullmodel_print_materialization_plan(options, view->ref, view->target_id,
                view->target_class, view->artifact_bytes, view->arch, view->tensor_count,
                view->total_tensor_bytes, view->collections, view->dtype_summary,
                view->role_coverage, view->missing_roles, view->selected_target);
        }
    } else if (options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        fullmodel_print_report_normal(options, "fullmodel-report", view->target_id,
            view->target_class, view->role_coverage,
            view->selected_target ? "selected-slice-not-full-runtime" :
                                    "missing-full-runtime-tensor-coverage",
            "tensor/source/artifact row required");
    } else {
        *handled = 0;
        return 0;
    }
    yvex_model_context_close(view->ctx);
    yvex_model_ref_clear(view->ref);
    return rc;
}

/* Render the detailed report after command-specific dispatch declines it. */
/* Purpose: Render fullmodel surface render audit from typed facts (`fullmodel_surface_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_surface_render_audit(fullmodel_surface_view *view)
{
    const yvex_cli_fullmodel_options *options = view->options;
    const yvex_fullmodel_collections *collections = view->collections;

    yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options->model);
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n",
                        view->ref->path ? view->ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", view->target_id);
    yvex_cli_out_writef(stdout, "target_class: %s\n", view->target_class);
    yvex_cli_out_writef(stdout, "source_artifact_class: %s\n", view->source_artifact_class);
    yvex_cli_out_writef(stdout, "target_artifact_class: %s\n", view->target_artifact_class);
    yvex_cli_out_writef(stdout, "artifact_exists: true\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", view->artifact_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n",
                        fullmodel_identity_status(view->ref, view->artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", view->tensor_count);
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\nmetadata_status: pass\n");
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(view->arch));
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(view->arch));
    yvex_cli_out_writef(stdout, "model_class: %s\n", view->model_class);
    yvex_cli_out_writef(stdout, "fullmodel_inventory: %s\n", view->inventory_status);
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "qtype_summary: %s\ndtype_summary: %s\n",
                        view->dtype_summary, view->dtype_summary);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", view->total_tensor_bytes);
    yvex_cli_out_writef(stdout, "estimated_cpu_resident_bytes: %llu\n",
                        view->total_tensor_bytes);
    yvex_cli_out_writef(stdout, "estimated_cuda_resident_bytes: %llu\n",
                        view->total_tensor_bytes);
    yvex_cli_out_writef(stdout,
        "estimated_kv_bytes: planned\nestimated_scratch_bytes: planned\nestimated_total_runtime_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", options->backend);
    yvex_cli_out_writef(stdout, "backend_placement_status: %s\n",
                        view->backend_placement_status);
    yvex_cli_out_writef(stdout, "cpu_placement: %s\ncuda_placement: %s\n",
                        view->cpu_placement, view->cuda_placement);
    yvex_cli_out_writef(stdout, "cuda_context_available: %s\n",
                        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "cuda_memory_status: %s\n",
        yvex_backend_cuda_context_available() ? "context-available-no-allocation" : "unavailable");
    yvex_cli_out_writef(stdout, "residency_plan: report-only-no-allocation\n");
    yvex_cli_out_writef(stdout, "tensor_collections_status: %s\n", view->role_coverage);
    yvex_cli_out_writef(stdout, "collection_detected: %s\n",
                        view->tensor_count ? "yes" : "no");
    yvex_cli_out_writef(stdout,
                        "collection_supported: partial\nruntime_consumer: unsupported\n");
    yvex_cli_out_writef(stdout, "embedding_tensors: %llu\n", collections->embedding);
    yvex_cli_out_writef(stdout, "normalization_tensors: %llu\n", collections->normalization);
    yvex_cli_out_writef(stdout, "attention_tensors: %llu\n", collections->attention);
    yvex_cli_out_writef(stdout, "kv_cache_requirements: planned\n");
    yvex_cli_out_writef(stdout, "mlp_tensors: %llu\n", collections->mlp);
    yvex_cli_out_writef(stdout, "moe_tensors: %llu\n", collections->moe);
    yvex_cli_out_writef(stdout, "output_tensors: %llu\n", collections->output);
    yvex_cli_out_writef(stdout, "tokenizer_tensors: %llu\n", collections->tokenizer);
    yvex_cli_out_writef(stdout, "unknown_tensors: %llu\n", collections->unknown);
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n", view->role_coverage);
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", view->missing_roles);
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", view->unsupported_roles);
    yvex_cli_out_writef(stdout,
        "runtime_blockers: full tensor set missing; attention projection tensors may be missing; MLP/MoE "
            "tensors may be missing; output head may be missing; real transformer prefill unsupported; real "
            "DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling "
            "unsupported; full model materialization not implemented\n");
    print_fullmodel_common_boundaries();
    yvex_cli_out_writef(stdout, "largest_tensor_report_limit: %llu\n", options->limit_tensors);
    fullmodel_print_largest(view->largest, view->largest_count);
    yvex_model_context_close(view->ctx);
    yvex_model_ref_clear(view->ref);
    return 0;
}

/* Purpose: Orchestrate the typed model artifacts surface fullmodel command request
 * (`yvex_model_artifacts_surface_fullmodel_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_model_artifacts_surface_fullmodel_command(int arg_count, char **args)
{
    yvex_cli_fullmodel_options options;
    fullmodel_surface_state state;
    int handled;
    int rc;

    memset(&options, 0, sizeof(options));
    rc = model_artifacts_fullmodel_options_parse(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;
    rc = fullmodel_source_only_dispatch(&options, &handled);
    if (handled) return rc;
    rc = fullmodel_surface_prepare(&options, &state);
    if (rc != YVEX_OK) return rc;
    rc = fullmodel_surface_dispatch(&state.view, &handled);
    if (handled) return rc;
    return fullmodel_surface_render_audit(&state.view);
}

/* Models command dispatch and help. */
