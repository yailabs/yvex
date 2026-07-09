/*
 * yvex_fullmodel_surface.c - fullmodel inventory/descriptor CLI surface helpers.
 * Owner: src/cli/model_artifacts
 * Owns: fullmodel option parsing, tensor inventory classification, descriptor rendering helpers.
 * Does not own: runtime generation, graph execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing fullmodel behavior.
 * Boundary: fullmodel reports are diagnostic/report-only unless a lower layer proves otherwise.
 */
#include "yvex_fullmodel_surface.h"

int fullmodel_string_is_empty(const char *text)
{
    return !text || !text[0];
}

static int fullmodel_parse_value_option(const char *flag,
                                        int arg_count,
                                        char **args,
                                        int *index,
                                        const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: fullmodel %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: fullmodel %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int fullmodel_phase_name_is_valid(const char *phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-coverage",
        "placement-plan",
        "memory-budget",
        "backend-preflight",
        "materialize-embedding",
        "materialize-normalization",
        "materialize-attention",
        "materialize-mlp",
        "materialize-moe",
        "materialize-output",
        "materialize-tokenizer",
        "cleanup",
        "complete",
        "failed"
    };
    unsigned int i;

    if (!phase || !phase[0]) return 0;
    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        if (strcmp(phase, phases[i]) == 0) return 1;
    }
    return 0;
}

static int fullmodel_command_is_materialize(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
}

static int fullmodel_command_is_descriptor(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
}

static int fullmodel_command_is_family_runtime(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
}

static int fullmodel_command_accepts_includes(const yvex_cli_fullmodel_options *options)
{
    return fullmodel_command_is_descriptor(options) ||
           fullmodel_command_is_family_runtime(options);
}

static int fullmodel_command_accepts_requirements(const yvex_cli_fullmodel_options *options)
{
    return fullmodel_command_is_materialize(options) ||
           fullmodel_command_is_descriptor(options);
}

int parse_fullmodel_options(int arg_count,
                                   char **args,
                                   yvex_cli_fullmodel_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->residency = "resident";
    options->format = "text";
    options->family = "auto";
    options->limit_tensors = 5ull;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    options->command = YVEX_FULLMODEL_COMMAND_REPORT;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_fullmodel_help(stdout);
        return 1;
    }
    if (arg_count < 3) {
        yvex_cli_out_writef(stderr, "yvex: fullmodel requires report, materialization-plan, materialize, descriptor, or family-runtime\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] [--plan-only] [--limit-bytes N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }
    if (strcmp(args[2], "report") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_REPORT;
    } else if (strcmp(args[2], "materialization-plan") == 0 ||
               strcmp(args[2], "plan") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    } else if (strcmp(args[2], "materialize") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    } else if (strcmp(args[2], "descriptor") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    } else if (strcmp(args[2], "family-runtime") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
    } else {
        yvex_cli_out_writef(stderr, "yvex: unknown fullmodel subcommand: %s\n", args[2]);
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--limit-tensors N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] [--plan-only] [--limit-bytes N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N]\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--model") == 0) {
            int rc = fullmodel_parse_value_option("--model", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(args[i], "--backend") == 0) {
            int rc = fullmodel_parse_value_option("--backend", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(args[i], "--target") == 0) {
            int rc = fullmodel_parse_value_option("--target", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->target = value;
        } else if (strcmp(args[i], "--registry") == 0) {
            int rc = fullmodel_parse_value_option("--registry", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(args[i], "--family") == 0) {
            int rc = fullmodel_parse_value_option("--family", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --family is only valid with family-runtime\n");
                return 2;
            }
            options->family = value;
        } else if (strcmp(args[i], "--residency") == 0) {
            int rc = fullmodel_parse_value_option("--residency", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "resident") != 0 &&
                strcmp(value, "host-staged") != 0 &&
                strcmp(value, "ssd-staged") != 0 &&
                strcmp(value, "hybrid") != 0 &&
                strcmp(value, "ssd-streamed") != 0 &&
                strcmp(value, "managed-memory") != 0 &&
                strcmp(value, "distributed") != 0) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --residency must be resident, host-staged, ssd-staged, hybrid, ssd-streamed, managed-memory, or distributed\n");
                return 2;
            }
            options->residency = value;
        } else if (strcmp(args[i], "--limit-tensors") == 0) {
            unsigned long long parsed = 0ull;
            int rc = fullmodel_parse_value_option("--limit-tensors", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --limit-tensors requires a positive integer\n");
                return 2;
            }
            options->limit_tensors = parsed > 16ull ? 16ull : parsed;
        } else if (strcmp(args[i], "--limit-bytes") == 0) {
            unsigned long long parsed = 0ull;
            int rc = fullmodel_parse_value_option("--limit-bytes", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --limit-bytes is only valid with materialize\n");
                return 2;
            }
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --limit-bytes requires a positive integer\n");
                return 2;
            }
            options->limit_bytes = parsed;
            options->has_limit_bytes = 1;
        } else if (strcmp(args[i], "--dry-run") == 0) {
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --dry-run is only valid with materialize\n");
                return 2;
            }
            options->dry_run = 1;
        } else if (strcmp(args[i], "--plan-only") == 0) {
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --plan-only is only valid with materialize\n");
                return 2;
            }
            options->plan_only = 1;
        } else if (strcmp(args[i], "--require-role") == 0) {
            int rc = fullmodel_parse_value_option("--require-role", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_accepts_requirements(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --require-role is only valid with materialize or descriptor\n");
                return 2;
            }
            options->require_role = value;
        } else if (strcmp(args[i], "--require-collection") == 0) {
            int rc = fullmodel_parse_value_option("--require-collection", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_accepts_requirements(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --require-collection is only valid with materialize or descriptor\n");
                return 2;
            }
            options->require_collection = value;
        } else if (strcmp(args[i], "--fail-after-phase") == 0) {
            int rc = fullmodel_parse_value_option("--fail-after-phase", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --fail-after-phase is only valid with materialize\n");
                return 2;
            }
            if (!fullmodel_phase_name_is_valid(value)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --fail-after-phase value is not a known materialize phase\n");
                return 2;
            }
            options->fail_after_phase = value;
        } else if (strcmp(args[i], "--report-dir") == 0) {
            int rc = fullmodel_parse_value_option("--report-dir", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --report-dir is only valid with materialize\n");
                return 2;
            }
            options->report_dir = value;
        } else if (strcmp(args[i], "--format") == 0) {
            int rc = fullmodel_parse_value_option("--format", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_descriptor(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --format is only valid with descriptor\n");
                return 2;
            }
            if (strcmp(value, "text") != 0) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel descriptor currently supports --format text only\n");
                return 2;
            }
            options->format = value;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            int rc = fullmodel_parse_value_option("--" "output", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-blockers is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_blockers = 1;
        } else if (strcmp(args[i], "--" "include-roles") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-roles is only valid with family-runtime\n");
                return 2;
            }
            options->include_roles = 1;
        } else if (strcmp(args[i], "--" "include-placement") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-placement is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_placement = 1;
        } else if (strcmp(args[i], "--" "include-graph") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-graph is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_graph = 1;
        } else if (strcmp(args[i], "--" "include-kv") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-kv is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_kv = 1;
        } else if (strcmp(args[i], "--" "include-logits") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-logits is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_logits = 1;
        } else if (strcmp(args[i], "--" "include-moe") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-moe is only valid with family-runtime\n");
                return 2;
            }
            options->include_moe = 1;
        } else if (strcmp(args[i], "--" "include-output") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --" "include-output is only valid with family-runtime\n");
                return 2;
            }
            options->include_output = 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown fullmodel option: %s\n", args[i]);
            return 2;
        }
    }
    if (!options->model) {
        const char *name = "report";
        if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
            name = "materialization-plan";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
            name = "materialize";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
            name = "descriptor";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
            name = "family-runtime";
        }
        yvex_cli_out_writef(stderr, "yvex: fullmodel %s requires --model FILE_OR_ALIAS\n",
                name);
        return 2;
    }
    return 0;
}

int fullmodel_file_size(const char *path,
                               unsigned long long *bytes)
{
    struct stat st;

    if (bytes) *bytes = 0ull;
    if (!path || stat(path, &st) != 0) return 0;
    if (bytes) *bytes = (unsigned long long)st.st_size;
    return 1;
}

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

int fullmodel_name_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

void fullmodel_csv_append(char *buf,
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

static void fullmodel_collection_add(unsigned long long *count,
                                     unsigned long long *bytes,
                                     const yvex_tensor_info *tensor)
{
    if (count) (*count)++;
    if (bytes && tensor) *bytes += tensor->storage_bytes;
}

void fullmodel_record_dtype(yvex_fullmodel_dtype_bucket buckets[32],
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

void fullmodel_dtype_summary(char *out,
                                    size_t out_cap,
                                    const yvex_fullmodel_dtype_bucket buckets[32],
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

void fullmodel_record_largest(yvex_fullmodel_largest_tensor top[16],
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

void fullmodel_classify_tensor(const yvex_tensor_info *tensor,
                                      yvex_fullmodel_collections *collections)
{
    const char *name;

    if (!tensor || !collections) return;
    name = tensor->name ? tensor->name : "";
    switch (tensor->role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
        fullmodel_collection_add(&collections->embedding, &collections->embedding_bytes, tensor);
        collections->has_token_embedding = 1;
        return;
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_output_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
        fullmodel_collection_add(&collections->output, &collections->output_bytes, tensor);
        collections->has_output_head = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_attention_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_Q:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_q = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_K:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_k = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_V:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_v = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_OUT:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_out = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_post_attention_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_GATE:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_gate = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_UP:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_up = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_DOWN:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_down = 1;
        return;
    case YVEX_TENSOR_ROLE_MOE_ROUTER:
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_router = 1;
        return;
    case YVEX_TENSOR_ROLE_MOE_EXPERT_GATE:
    case YVEX_TENSOR_ROLE_MOE_EXPERT_UP:
    case YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN:
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_expert = 1;
        return;
    default:
        break;
    }

    if (fullmodel_name_has(name, "token_embd") || fullmodel_name_has(name, "embed")) {
        fullmodel_collection_add(&collections->embedding, &collections->embedding_bytes, tensor);
        collections->has_token_embedding = 1;
    } else if (fullmodel_name_has(name, "attn_norm") ||
               fullmodel_name_has(name, "input_layernorm")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_attention_norm = 1;
    } else if (fullmodel_name_has(name, "ffn_norm") ||
               fullmodel_name_has(name, "post_attention_layernorm")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_post_attention_norm = 1;
    } else if (fullmodel_name_has(name, "attn_q") || fullmodel_name_has(name, "q_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_q = 1;
    } else if (fullmodel_name_has(name, "attn_k") || fullmodel_name_has(name, "k_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_k = 1;
    } else if (fullmodel_name_has(name, "attn_v") || fullmodel_name_has(name, "v_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_v = 1;
    } else if (fullmodel_name_has(name, "attn_output") || fullmodel_name_has(name, "o_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_out = 1;
    } else if (fullmodel_name_has(name, "ffn_gate") || fullmodel_name_has(name, "gate_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_gate = 1;
    } else if (fullmodel_name_has(name, "ffn_up") || fullmodel_name_has(name, "up_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_up = 1;
    } else if (fullmodel_name_has(name, "ffn_down") || fullmodel_name_has(name, "down_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_down = 1;
    } else if (fullmodel_name_has(name, "router") || fullmodel_name_has(name, "gate.weight")) {
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_router = 1;
    } else if (fullmodel_name_has(name, "expert")) {
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_expert = 1;
    } else if (fullmodel_name_has(name, "output_norm") || fullmodel_name_has(name, "norm.weight")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_output_norm = 1;
    } else if (strcmp(name, "output.weight") == 0 || fullmodel_name_has(name, "lm_head")) {
        fullmodel_collection_add(&collections->output, &collections->output_bytes, tensor);
        collections->has_output_head = 1;
    } else {
        fullmodel_collection_add(&collections->unknown, &collections->unknown_bytes, tensor);
    }
}

int fullmodel_is_selected_target(const char *text)
{
    return text &&
           (strcmp(text, "deepseek4-v4-flash-selected-embed") == 0 ||
            strcmp(text, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0);
}

void print_fullmodel_common_boundaries(void)
{
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "full_model_materialization: planned\n");
    yvex_cli_out_writef(stdout, "full_runtime_descriptor: planned\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
}

static int fullmodel_descriptor_tensor_matches(const yvex_tensor_info *tensor,
                                               const char *role)
{
    const char *name;

    if (!tensor || !role) return 0;
    name = tensor->name ? tensor->name : "";
    if (strcmp(role, "token_embedding") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING ||
               fullmodel_name_has(name, "token_embd") ||
               fullmodel_name_has(name, "embed");
    }
    if (strcmp(role, "attention_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM ||
               fullmodel_name_has(name, "attn_norm") ||
               fullmodel_name_has(name, "input_layernorm");
    }
    if (strcmp(role, "post_attention_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_NORM ||
               fullmodel_name_has(name, "ffn_norm") ||
               fullmodel_name_has(name, "post_attention_layernorm");
    }
    if (strcmp(role, "final_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_OUTPUT_NORM ||
               fullmodel_name_has(name, "output_norm") ||
               fullmodel_name_has(name, "final_norm");
    }
    if (strcmp(role, "q_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_Q ||
               fullmodel_name_has(name, "attn_q") ||
               fullmodel_name_has(name, "q_proj");
    }
    if (strcmp(role, "k_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_K ||
               fullmodel_name_has(name, "attn_k") ||
               fullmodel_name_has(name, "k_proj");
    }
    if (strcmp(role, "v_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_V ||
               fullmodel_name_has(name, "attn_v") ||
               fullmodel_name_has(name, "v_proj");
    }
    if (strcmp(role, "o_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_OUT ||
               fullmodel_name_has(name, "attn_output") ||
               fullmodel_name_has(name, "o_proj");
    }
    if (strcmp(role, "mlp_gate") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_GATE ||
               fullmodel_name_has(name, "ffn_gate") ||
               fullmodel_name_has(name, "gate_proj");
    }
    if (strcmp(role, "mlp_up") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_UP ||
               fullmodel_name_has(name, "ffn_up") ||
               fullmodel_name_has(name, "up_proj");
    }
    if (strcmp(role, "mlp_down") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_DOWN ||
               fullmodel_name_has(name, "ffn_down") ||
               fullmodel_name_has(name, "down_proj");
    }
    if (strcmp(role, "moe_router") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_ROUTER ||
               fullmodel_name_has(name, "router");
    }
    if (strcmp(role, "moe_expert_gate") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_GATE ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "gate"));
    }
    if (strcmp(role, "moe_expert_up") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_UP ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "up"));
    }
    if (strcmp(role, "moe_expert_down") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "down"));
    }
    if (strcmp(role, "output_head") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD ||
               strcmp(name, "output.weight") == 0 ||
               fullmodel_name_has(name, "lm_head");
    }
    if (strcmp(role, "unknown") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_UNKNOWN;
    }
    return 0;
}

const yvex_tensor_info *fullmodel_descriptor_find_tensor(yvex_cli_tokenizer_context *ctx,
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

static const char *fullmodel_descriptor_role_collection(const char *role)
{
    if (!role) return "unknown";
    if (strcmp(role, "token_embedding") == 0) return "embedding";
    if (strcmp(role, "attention_norm") == 0 ||
        strcmp(role, "post_attention_norm") == 0 ||
        strcmp(role, "final_norm") == 0) return "normalization";
    if (strcmp(role, "q_projection") == 0 ||
        strcmp(role, "k_projection") == 0 ||
        strcmp(role, "v_projection") == 0 ||
        strcmp(role, "o_projection") == 0) return "attention";
    if (strcmp(role, "mlp_gate") == 0 ||
        strcmp(role, "mlp_up") == 0 ||
        strcmp(role, "mlp_down") == 0) return "mlp";
    if (strcmp(role, "moe_router") == 0 ||
        strcmp(role, "moe_expert_gate") == 0 ||
        strcmp(role, "moe_expert_up") == 0 ||
        strcmp(role, "moe_expert_down") == 0) return "moe";
    if (strcmp(role, "output_head") == 0) return "output";
    if (strcmp(role, "tokenizer_metadata") == 0) return "tokenizer-runtime-input";
    return "unknown";
}

static const char *fullmodel_descriptor_role_residency(const char *role,
                                                       const char *backend,
                                                       int present)
{
    if (!present) return "not-planned";
    if (role && strcmp(role, "tokenizer_metadata") == 0) return "host-runtime-metadata";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident-planned" : "cpu-resident-planned";
}

static void fullmodel_print_descriptor_role(yvex_cli_tokenizer_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *backend)
{
    const yvex_tensor_info *tensor = NULL;
    char dims[128];
    int present = 0;

    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        present = collections && collections->has_tokenizer_metadata;
    } else {
        tensor = fullmodel_descriptor_find_tensor(ctx, role);
        present = tensor != NULL;
    }
    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    yvex_cli_out_writef(stdout, "role.%s.status: %s\n", role ? role : "unknown", present ? "present" : "missing");
    yvex_cli_out_writef(stdout, "role.%s.tensor: %s\n", role ? role : "unknown",
           tensor && tensor->name ? tensor->name : present ? "metadata" : "none");
    yvex_cli_out_writef(stdout, "role.%s.shape: %s\n", role ? role : "unknown",
           tensor ? dims : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.dtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.qtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.bytes: %llu\n", role ? role : "unknown",
           tensor ? tensor->storage_bytes : 0ull);
    yvex_cli_out_writef(stdout, "role.%s.collection: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_collection(role));
    yvex_cli_out_writef(stdout, "role.%s.residency_expectation: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_residency(role, backend, present));
    yvex_cli_out_writef(stdout, "role.%s.runtime_consumer: %s\n", role ? role : "unknown",
           present ? "planned" : "blocked-missing-role");
}

static void fullmodel_print_descriptor_collection(const char *name,
                                                  unsigned long long count,
                                                  unsigned long long bytes,
                                                  int required_for_prefill,
                                                  int required_for_decode,
                                                  int required_for_logits,
                                                  int required_for_generation,
                                                  const char *runtime_consumer,
                                                  const char *blocker)
{
    yvex_cli_out_writef(stdout, "collection.%s.status: %s\n", name, count > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "collection.%s.tensor_count: %llu\n", name, count);
    yvex_cli_out_writef(stdout, "collection.%s.byte_count: %llu\n", name, bytes);
    yvex_cli_out_writef(stdout, "collection.%s.required_for_prefill: %s\n", name, required_for_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_decode: %s\n", name, required_for_decode ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_logits: %s\n", name, required_for_logits ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_generation: %s\n", name, required_for_generation ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.runtime_consumer: %s\n", name, runtime_consumer ? runtime_consumer : "planned");
    yvex_cli_out_writef(stdout, "collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

static void fullmodel_print_descriptor_phase(unsigned int index,
                                             const char *name,
                                             const char *status)
{
    yvex_cli_out_writef(stdout, "descriptor_phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "descriptor_phase.%u.status: %s\n", index, status ? status : "planned");
}

void fullmodel_print_descriptor_phases(const char *role_status,
                                              const char *collection_status,
                                              const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-map",
        "collection-map",
        "shape-requirements",
        "residency-requirements",
        "graph-requirements",
        "prefill-requirements",
        "kv-requirements",
        "decode-requirements",
        "logits-requirements",
        "sampling-requirements",
        "tokenizer-requirements",
        "backend-requirements",
        "blocker-report",
        "descriptor-build",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "skipped";
        } else if (strcmp(phases[i], "role-map") == 0) {
            status = role_status ? role_status : "partial";
        } else if (strcmp(phases[i], "collection-map") == 0) {
            status = collection_status ? collection_status : "partial";
        } else if (strcmp(phases[i], "residency-requirements") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "backend-requirements") == 0) {
            status = "planned";
        } else if (strcmp(phases[i], "prefill-requirements") == 0 ||
                   strcmp(phases[i], "kv-requirements") == 0 ||
                   strcmp(phases[i], "decode-requirements") == 0 ||
                   strcmp(phases[i], "logits-requirements") == 0 ||
                   strcmp(phases[i], "sampling-requirements") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        fullmodel_print_descriptor_phase(i, phases[i], status);
    }
}

static void fullmodel_print_descriptor_graph_requirements(const yvex_fullmodel_collections *collections)
{
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);

    yvex_cli_out_writef(stdout, "graph_requirements_status: blocked\n");
    yvex_cli_out_writef(stdout, "required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp-gate-up-down,activation,moe-router,expert-dispatch,expert-accumulation,final-norm,output-head-projection\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: full-transformer-attention,real-layer-scheduler,real-moe-router,real-expert-dispatch,real-output-head-projection\n");
    yvex_cli_out_writef(stdout, "required_backend_ops: tensor-read,rmsnorm,matmul,rope,attention,softmax,activation,residual-add,kv-read,kv-write\n");
    yvex_cli_out_writef(stdout, "unsupported_backend_ops: full-transformer-runtime-integration,real-attention-backed-kv,real-output-head-logits\n");
    yvex_cli_out_writef(stdout, "graph.embedding_lookup: %s\n",
           collections && collections->has_token_embedding ? "planned-real-tensor" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.rmsnorm: %s\n",
           fullmodel_has_normalization_collection(collections) ? "implemented-selected-segment" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.q_projection: %s\n", collections && collections->has_attention_q ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.k_projection: %s\n", collections && collections->has_attention_k ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.v_projection: %s\n", collections && collections->has_attention_v ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.rope_position_op: implemented-primitive\n");
    yvex_cli_out_writef(stdout, "graph.attention_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph.full_transformer_attention: %s\n", has_attention ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.o_projection: %s\n", collections && collections->has_attention_out ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.residual_add: planned\n");
    yvex_cli_out_writef(stdout, "graph.mlp_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph.full_transformer_mlp: %s\n", has_mlp ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.moe_router: %s\n",
           collections && collections->has_moe_router ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.expert_dispatch: %s\n",
           collections && collections->has_moe_expert ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.output_head_projection: %s\n",
           collections && collections->has_output_head ? "planned" : "missing-tensor");
}

void fullmodel_print_descriptor_report(const yvex_cli_fullmodel_options *options,
                                              yvex_model_ref *ref,
                                              yvex_cli_tokenizer_context *ctx,
                                              const char *target_id,
                                              const char *target_class,
                                              unsigned long long artifact_bytes,
                                              yvex_arch arch,
                                              unsigned long long tensor_count,
                                              unsigned long long total_tensor_bytes,
                                              const yvex_fullmodel_collections *collections,
                                              const char *role_coverage,
                                              const char *missing_roles,
                                              const char *unsupported_roles,
                                              int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    int descriptor_complete = role_coverage &&
                              (strcmp(role_coverage, "complete") == 0 ||
                               strcmp(role_coverage, "observed") == 0);
    const char *descriptor_status = selected_target ? "partial" :
                                    (descriptor_complete ? "complete" : "partial");
    const char *materialization_plan_status = selected_target ? "partial" : "ready";
    const char *materialization_proof_status = selected_target ? "refused-selected-runtime-slice" :
                                               (descriptor_complete
                                                    ? "available-controlled-tiny-proof"
                                                    : "blocked-missing-roles");
    const char *full_materialization = selected_target ? "refused-selected-runtime-slice" :
                                      (descriptor_complete
                                           ? "controlled-tiny-proof-available"
                                           : "planned");
    unsigned long long cuda_bytes = strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull;
    unsigned long long cpu_bytes = strcmp(backend, "cuda") == 0 ? 0ull : total_tensor_bytes;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    yvex_cli_out_writef(stdout, "fullmodel: descriptor\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-descriptor\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "format: %s\n", options && options->format ? options->format : "text");
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "materialization_plan_status: %s\n", materialization_plan_status);
    yvex_cli_out_writef(stdout, "materialization_proof_status: %s\n", materialization_proof_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(arch));
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(arch));
    yvex_cli_out_writef(stdout, "model_class: %s\n", selected_target ? "selected-runtime-slice" : "descriptor-only-candidate");
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "full_model_materialization: %s\n", full_materialization);
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "tensor_role_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "tensor_collection_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n", descriptor_complete ? "complete" : (role_coverage ? role_coverage : "partial"));
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    yvex_cli_out_writef(stdout, "unknown_role_count: %llu\n", collections ? collections->unknown : 0ull);

    fullmodel_print_descriptor_role(ctx, collections, "token_embedding", backend);
    fullmodel_print_descriptor_role(ctx, collections, "attention_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "post_attention_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "final_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "q_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "k_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "v_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "o_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_gate", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_up", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_down", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_router", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_gate", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_up", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_down", backend);
    fullmodel_print_descriptor_role(ctx, collections, "output_head", backend);
    fullmodel_print_descriptor_role(ctx, collections, "tokenizer_metadata", backend);
    fullmodel_print_descriptor_role(ctx, collections, "unknown", backend);

    yvex_cli_out_writef(stdout, "embedding_descriptor: %s\n", collections && collections->embedding > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "normalization_descriptor: %s\n", collections && collections->normalization > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "attention_descriptor: %s\n", fullmodel_has_attention_collection(collections) ? "present" : "missing");
    yvex_cli_out_writef(stdout, "mlp_descriptor: %s\n", fullmodel_has_mlp_collection(collections) ? "present" : "missing");
    yvex_cli_out_writef(stdout, "moe_descriptor: %s\n", collections && collections->moe > 0ull ? "present" : "planned-or-missing");
    yvex_cli_out_writef(stdout, "output_descriptor: %s\n", collections && collections->output > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "tokenizer_descriptor: %s\n", collections && collections->has_tokenizer_metadata ? "present" : "missing");
    yvex_cli_out_writef(stdout, "kv_descriptor: unsupported-real-attention-backed-kv\n");

    fullmodel_print_descriptor_collection("embedding",
                                          collections ? collections->embedding : 0ull,
                                          collections ? collections->embedding_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          collections && collections->embedding > 0ull ? "none" : "embedding collection missing");
    fullmodel_print_descriptor_collection("normalization",
                                          collections ? collections->normalization : 0ull,
                                          collections ? collections->normalization_bytes : 0ull,
                                          1, 1, 1, 1, "planned",
                                          collections && collections->normalization > 0ull ? "none" : "normalization collection missing");
    fullmodel_print_descriptor_collection("attention",
                                          collections ? collections->attention : 0ull,
                                          collections ? collections->attention_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          fullmodel_has_attention_collection(collections) ? "none" : "attention Q/K/V/O tensors missing");
    fullmodel_print_descriptor_collection("mlp",
                                          collections ? collections->mlp : 0ull,
                                          collections ? collections->mlp_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          fullmodel_has_mlp_collection(collections) ? "none" : "MLP tensors missing");
    fullmodel_print_descriptor_collection("moe",
                                          collections ? collections->moe : 0ull,
                                          collections ? collections->moe_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          collections && collections->moe > 0ull ? "none" : "MoE tensors missing or not identified");
    fullmodel_print_descriptor_collection("output",
                                          collections ? collections->output : 0ull,
                                          collections ? collections->output_bytes : 0ull,
                                          0, 1, 1, 1, "planned",
                                          collections && collections->output > 0ull ? "none" : "output head missing");
    fullmodel_print_descriptor_collection("tokenizer-runtime-input",
                                          collections ? collections->tokenizer : 0ull,
                                          collections ? collections->tokenizer_bytes : 0ull,
                                          1, 1, 1, 1, "planned",
                                          collections && collections->has_tokenizer_metadata ? "none" : "tokenizer metadata missing");
    fullmodel_print_descriptor_collection("kv-cache-runtime", 0ull, 0ull,
                                          1, 1, 0, 1, "unsupported",
                                          "real attention-backed KV writes unsupported");
    fullmodel_print_descriptor_collection("unknown",
                                          collections ? collections->unknown : 0ull,
                                          collections ? collections->unknown_bytes : 0ull,
                                          0, 0, 0, 0, "unsupported",
                                          collections && collections->unknown > 0ull ? "unknown tensor role" : "none");

    fullmodel_print_descriptor_graph_requirements(collections);

    yvex_cli_out_writef(stdout, "prefill_descriptor: unsupported-full-transformer-prefill\n");
    yvex_cli_out_writef(stdout, "prefill.requires_embedding: true\n");
    yvex_cli_out_writef(stdout, "prefill.requires_attention_qkv: true\n");
    yvex_cli_out_writef(stdout, "prefill.requires_real_kv_writes: true\n");
    yvex_cli_out_writef(stdout, "prefill.requires_mlp_or_moe: true\n");
    yvex_cli_out_writef(stdout, "prefill.requires_layer_scheduler: true\n");
    yvex_cli_out_writef(stdout, "prefill.current_status: unsupported\n");
    yvex_cli_out_writef(stdout, "prefill.blocker: real transformer prefill not implemented\n");
    yvex_cli_out_writef(stdout, "decode_descriptor: unsupported-full-model-decode\n");
    yvex_cli_out_writef(stdout, "decode.mode_required: baseline-autoregressive\n");
    yvex_cli_out_writef(stdout, "decode.requires_prefill_state: true\n");
    yvex_cli_out_writef(stdout, "decode.requires_kv_read: true\n");
    yvex_cli_out_writef(stdout, "decode.requires_layer_execution: true\n");
    yvex_cli_out_writef(stdout, "decode.current_status: unsupported\n");
    yvex_cli_out_writef(stdout, "decode.blocker: full model decode not implemented\n");
    yvex_cli_out_writef(stdout, "logits_descriptor: unsupported-real-output-head-logits\n");
    yvex_cli_out_writef(stdout, "sampling_descriptor: unsupported-real-vocabulary-sampling\n");

    yvex_cli_out_writef(stdout, "residency_requirements_status: planned\n");
    yvex_cli_out_writef(stdout, "residency_plan: descriptor-only-no-allocation\n");
    yvex_cli_out_writef(stdout, "cpu_resident_required_bytes: %llu\n", cpu_bytes);
    yvex_cli_out_writef(stdout, "cuda_resident_required_bytes: %llu\n", cuda_bytes);
    yvex_cli_out_writef(stdout, "host_staged_required_bytes: %llu\n", strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout, "ssd_staged_required_bytes: planned\n");
    yvex_cli_out_writef(stdout, "kv_required_bytes: planned\n");
    yvex_cli_out_writef(stdout, "scratch_required_bytes: planned\n");

    yvex_cli_out_writef(stdout, "context_requirements_status: planned\n");
    yvex_cli_out_writef(stdout, "max_context: metadata-or-unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_policy: planned\n");
    yvex_cli_out_writef(stdout, "position_policy: rope-or-family-specific-planned\n");
    yvex_cli_out_writef(stdout, "rope_policy: planned\n");

    yvex_cli_out_writef(stdout, "kv_requirements_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-full-transformer-kv\n");
    yvex_cli_out_writef(stdout, "kv.required: true\n");
    yvex_cli_out_writef(stdout, "kv.real_attention_writes: false\n");
    yvex_cli_out_writef(stdout, "kv.runtime_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");

    yvex_cli_out_writef(stdout, "logits_requirements_status: unsupported\n");
    yvex_cli_out_writef(stdout, "output_head_present: %s\n", collections && collections->has_output_head ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    yvex_cli_out_writef(stdout, "output_head_dtype: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? yvex_dtype_name(fullmodel_descriptor_find_tensor(ctx, "output_head")->dtype)
               : "unknown");
    yvex_cli_out_writef(stdout, "vocab_size: %s\n", collections && collections->has_output_head ? "from-output-head-shape" : "unknown");
    yvex_cli_out_writef(stdout, "logits_buffer_required: true\n");
    yvex_cli_out_writef(stdout, "real_output_head_logits: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "logits.blocker: real output-head logits runtime unsupported\n");

    yvex_cli_out_writef(stdout, "tokenizer_requirements_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked");
    yvex_cli_out_writef(stdout, "tokenizer_metadata_present: %s\n",
           collections && collections->has_tokenizer_metadata ? "true" : "false");
    yvex_cli_out_writef(stdout, "special_token_policy: planned\n");
    yvex_cli_out_writef(stdout, "eos_backed_stop: unsupported\n");
    yvex_cli_out_writef(stdout, "stop_token_text_matching: unsupported\n");
    yvex_cli_out_writef(stdout, "tokenizer_quality_generation: unsupported\n");

    yvex_cli_out_writef(stdout, "backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    yvex_cli_out_writef(stdout, "backend.cpu.available: true\n");
    yvex_cli_out_writef(stdout, "backend.cuda.available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.memory_known: %s\n", fit.memory_known ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend.fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend.fit_reason: %s\n", fit.fit_reason);
    yvex_cli_out_writef(stdout, "backend.primitive_rope: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "backend.primitive_attention: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "backend.primitive_matmul: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "backend.primitive_mlp: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "backend.full_transformer_integration: unsupported\n");
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");

    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n",
           selected_target
               ? "full runtime tensor set incomplete; attention Q/K/V/O tensors missing; MLP/MoE tensors missing; output head missing; real transformer prefill unsupported; real attention-backed KV writes unsupported; full model decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported; full model execution unsupported"
               : "real transformer prefill unsupported; real attention-backed KV writes unsupported; full model decode unsupported; real output-head logits runtime unsupported; real vocabulary sampling unsupported; full model execution unsupported");
    yvex_cli_out_writef(stdout, "descriptor_blockers: %s\n",
           selected_target
               ? "selected-runtime-slice is partial descriptor only"
               : "runtime family adapter boundary remains planned");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    fullmodel_print_descriptor_phases(descriptor_status, descriptor_status, NULL);
}
