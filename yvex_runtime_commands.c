/*
 * yvex_runtime_commands.c - Runtime command adapters.
 *
 * This file owns argv/output adapters for backend, token, KV, prefill, run,
 * session, and accepted-only chat diagnostics. Runtime state and lifecycle
 * behavior remain owned by yvex_runtime.c and related modules.
 */

#include "yvex_command_private.h"

static int print_special_id_line(const char *name, int (*fn)(const yvex_tokenizer *, unsigned int *), const yvex_tokenizer *tokenizer)
{
    unsigned int id;
    int rc = fn(tokenizer, &id);

    if (rc == YVEX_OK) {
        printf("%s: %u\n", name, id);
    } else {
        printf("%s: absent\n", name);
    }
    return YVEX_OK;
}

static void print_backend_capability(const yvex_backend *backend, yvex_backend_capability capability)
{
    printf("  %s: %s\n",
           yvex_backend_capability_name(capability),
           yvex_backend_supports(backend, capability) ? "yes" : "no");
}

static const char *yes_no(int value)
{
    return value ? "yes" : "no";
}

static int command_backend(int argc, char **argv)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            yvex_cli_print_command_help(stdout, yvex_cli_find_command("backend"));
            return 0;
        }
        fprintf(stderr, "yvex: backend requires cpu or cuda\n");
        fprintf(stderr, "usage: yvex backend cpu|cuda\n");
        return 2;
    }

    if (strcmp(argv[2], "cpu") == 0) {
        options.kind = YVEX_BACKEND_KIND_CPU;
    } else if (strcmp(argv[2], "cuda") == 0) {
        options.kind = YVEX_BACKEND_KIND_CUDA;
    } else {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", argv[2]);
        fprintf(stderr, "Try 'yvex help backend' for usage.\n");
        return 2;
    }

    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("backend: %s\n", argv[2]);
        printf("status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: backend-unsupported\n");
        return 5;
    }
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(backend)));
    printf("status: %s\n", yvex_backend_status_name(yvex_backend_status_of(backend)));
    if (yvex_backend_get_device_info(backend, &device_info, &err) == YVEX_OK &&
        yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CUDA) {
        printf("device: %d\n", device_info.device_index);
        printf("name: %s\n", device_info.name ? device_info.name : "");
        printf("compute_capability: %d.%d\n",
               device_info.compute_capability_major,
               device_info.compute_capability_minor);
        printf("memory:\n");
        printf("  free_bytes: %llu\n", device_info.free_memory_bytes);
        printf("  total_bytes: %llu\n", device_info.total_memory_bytes);
        printf("  allocated_bytes: %llu\n", stats.allocated_bytes);
        printf("  allocation_count: %llu\n", stats.allocation_count);
        printf("  peak_allocated_bytes: %llu\n", stats.peak_allocated_bytes);
    } else {
    printf("memory:\n");
    printf("  allocated_bytes: %llu\n", stats.allocated_bytes);
    printf("  allocation_count: %llu\n", stats.allocation_count);
    printf("  peak_allocated_bytes: %llu\n", stats.peak_allocated_bytes);
    }
    printf("capabilities:\n");
    print_backend_capability(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC);
    print_backend_capability(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_EMBED);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_MATMUL);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_MLP);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_RMS_NORM);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_ROPE);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_ATTENTION);
    printf("status: backend-ready\n");

    yvex_backend_close(backend);
    return 0;
}

static int command_cuda_info(int argc, char **argv)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_device_info info;
    yvex_error err;
    int rc;

    if (argc != 2 && !(argc == 3 && (strcmp(argv[2], "--help") == 0 ||
                                    strcmp(argv[2], "-h") == 0))) {
        fprintf(stderr, "usage: yvex cuda-info\n");
        return 2;
    }
    if (argc == 3) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("cuda-info"));
        return 0;
    }

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("cuda: unavailable\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: cuda-unavailable\n");
        return 5;
    }
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_get_device_info(backend, &info, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("cuda: available\n");
    printf("device_count: >=1\n");
    printf("\n");
    printf("device %d:\n", info.device_index);
    printf("  name: %s\n", info.name ? info.name : "");
    printf("  compute_capability: %d.%d\n",
           info.compute_capability_major,
           info.compute_capability_minor);
    printf("  global_memory_bytes: %llu\n", info.global_memory_bytes);
    printf("  free_memory_bytes: %llu\n", info.free_memory_bytes);
    printf("  total_memory_bytes: %llu\n", info.total_memory_bytes);
    printf("  unified_addressing: %s\n", yes_no(info.unified_addressing));
    printf("  managed_memory: %s\n", yes_no(info.managed_memory));
    printf("\n");
    printf("status: cuda-info\n");
    yvex_backend_close(backend);
    return 0;
}

static int command_tokenizer(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const char *chat_template;
    unsigned long long chat_template_len;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            yvex_cli_print_command_help(stdout, yvex_cli_find_command("tokenizer"));
            return 0;
        }
        fprintf(stderr, "yvex: tokenizer requires exactly one path\n");
        fprintf(stderr, "usage: yvex tokenizer <path>\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("format: gguf\n");
    printf("architecture: %s\n", yvex_arch_name(yvex_model_arch(ctx.model)));
    printf("model_name: %s\n", yvex_model_name(ctx.model));
    printf("tokenizer_model: %s\n", yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(ctx.tokenizer)));
    printf("support: %s\n", yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    printf("vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    (void)print_special_id_line("bos_token_id", yvex_tokenizer_bos_id, ctx.tokenizer);
    (void)print_special_id_line("eos_token_id", yvex_tokenizer_eos_id, ctx.tokenizer);
    (void)print_special_id_line("unk_token_id", yvex_tokenizer_unk_id, ctx.tokenizer);
    if (yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK) {
        (void)chat_template;
        printf("chat_template: present-unsupported\n");
    } else {
        printf("chat_template: absent\n");
    }
    printf("status: tokenizer-descriptor\n");

    close_tokenizer_context(&ctx);
    return 0;
}

static int command_tokenize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_tokens tokens;
    yvex_error err;
    const char *text = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("tokenize"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--text") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = argv[++i];
        } else if (strcmp(argv[i], "--pieces") == 0 || strcmp(argv[i], "--no-bos") == 0 || strcmp(argv[i], "--eos") == 0) {
            /* Accepted for tokenizer layer CLI shape; fixture tokenization has no implicit BOS/EOS. */
        } else {
            fprintf(stderr, "yvex: unknown tokenize option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help tokenize' for usage.\n");
            return 2;
        }
    }
    if (!text) {
        fprintf(stderr, "yvex: tokenize requires --text\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, text, &tokens, &err);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("tokens: %llu\n", tokens.len);
    print_token_ids(&tokens);
    printf("pieces:\n");
    for (i = 0; (unsigned long long)i < tokens.len; ++i) {
        const yvex_token_info *token = yvex_tokenizer_token_at(ctx.tokenizer, tokens.ids[i]);
        printf("  %u ", tokens.ids[i]);
        print_quoted_bytes(token ? token->text : "", token ? token->text_len : 0);
        printf("\n");
    }
    printf("status: tokenized\n");

    yvex_tokens_free(&tokens);
    close_tokenizer_context(&ctx);
    return 0;
}

static void print_token_input_tokens(const yvex_token_input *input)
{
    unsigned long long i;

    for (i = 0; input && i < input->token_count; ++i) {
        printf("token_%llu: %u\n", i, input->tokens[i]);
    }
}

static void fill_kv_demo_values(float *values,
                                unsigned long long value_count,
                                unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        values[i] = (float)((position * 1000ull) + i);
    }
}

static unsigned long long checksum_kv_values(const float *values,
                                             unsigned long long value_count)
{
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        unsigned long long v = (unsigned long long)values[i];
        checksum ^= v + (i << 8u);
        checksum *= 1099511628211ull;
    }
    return checksum;
}

void print_token_input_summary(const yvex_token_input *input,
                                      const char *status,
                                      const char *bounds_status,
                                      unsigned long long selected_index,
                                      unsigned int selected_token,
                                      int has_selected)
{
    printf("token_input_status: %s\n", status ? status : "fail");
    printf("token_input_kind: %s\n",
           input ? yvex_token_input_kind_name(input->kind) : "unknown");
    printf("token_count: %llu\n", input ? input->token_count : 0ull);
    if (input) {
        printf("selected_token_index: %llu\n", selected_index);
    }
    if (has_selected) {
        printf("selected_token_id: %u\n", selected_token);
    } else if (input) {
        printf("selected_token_id: unavailable\n");
    }
    printf("token_bounds_status: %s\n", bounds_status ? bounds_status : "not-checked");
}

int cli_token_input_vocab_from_model(const char *path,
                                            unsigned long long *out_vocab_size,
                                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *tensor;
    yvex_tokenizer *tokenizer = NULL;
    int rc;

    if (!path || !out_vocab_size) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cli_token_input_vocab_from_model",
                       "path and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out_vocab_size = 0ull;
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (tensor && tensor->rank == 2 && tensor->dims[1] > 0ull) {
        *out_vocab_size = tensor->dims[1];
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_tokenizer_from_gguf(&tokenizer, ctx.gguf, ctx.model, err);
    if (rc == YVEX_OK && yvex_tokenizer_vocab_size(tokenizer) > 0ull) {
        *out_vocab_size = yvex_tokenizer_vocab_size(tokenizer);
        yvex_tokenizer_close(tokenizer);
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    yvex_tokenizer_close(tokenizer);
    close_model_context(&ctx);
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cli_token_input_vocab_from_model",
                   "tokenizer-metadata-missing");
    return YVEX_ERR_UNSUPPORTED;
}

static int command_input(int argc, char **argv)
{
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_token_input input;
    yvex_tokens tokens;
    yvex_error err;
    const char *subcommand;
    const char *model_arg = NULL;
    const char *tokens_text = NULL;
    const char *prompt_text = NULL;
    unsigned long long vocab_size = 0ull;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));
    memset(&tokens, 0, sizeof(tokens));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("input"));
        return argc >= 3 ? 0 : 2;
    }

    subcommand = argv[2];
    if (strcmp(subcommand, "tokens") != 0 && strcmp(subcommand, "prompt") != 0) {
        fprintf(stderr, "yvex: input requires tokens or prompt\n");
        fprintf(stderr, "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0 && strcmp(subcommand, "tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--text") == 0 && strcmp(subcommand, "prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires TEXT\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown input option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help input' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: input requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (strcmp(subcommand, "tokens") == 0 && !tokens_text) {
        fprintf(stderr, "yvex: input tokens requires --tokens IDS\n");
        return 2;
    }
    if (strcmp(subcommand, "prompt") == 0 && !prompt_text) {
        fprintf(stderr, "yvex: input prompt requires --text TEXT\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&ref, "input");
    if (rc != YVEX_OK) {
        printf("token_input: %s\n", subcommand);
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("identity_status: fail\n");
        print_token_input_summary(NULL, "fail", "not-checked", 0ull, 0u, 0);
        printf("prefill_ready: false\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    if (strcmp(subcommand, "tokens") == 0) {
        rc = yvex_token_input_parse_explicit(tokens_text, &input, &err);
        if (rc == YVEX_OK) {
            rc = cli_token_input_vocab_from_model(ref.path, &vocab_size, &err);
        }
        if (rc == YVEX_OK) {
            rc = yvex_token_input_validate_bounds(&input, vocab_size, &err);
        }

        printf("token_input: tokens\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("identity_status: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered");
        print_token_input_summary(&input,
                                  rc == YVEX_OK ? "pass" : "fail",
                                  rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                                  0ull,
                                  input.token_count > 0ull ? input.tokens[0] : 0u,
                                  input.token_count > 0ull);
        printf("vocab_size: %llu\n", vocab_size);
        print_token_input_tokens(&input);
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
        yvex_model_ref_clear(&ref);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        return 0;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: not-checked\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenizer_from_gguf(&ctx.tokenizer, ctx.gguf, ctx.model, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: missing\n");
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }
    if (yvex_tokenizer_support_of(ctx.tokenizer) != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: unsupported\n");
        printf("tokenizer_support: %s\n",
               yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_tokenizer_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, prompt_text, &tokens, &err);
    if (rc == YVEX_OK) {
        rc = yvex_token_input_from_ids(YVEX_TOKEN_INPUT_PROMPT_TEXT,
                                       tokens.ids,
                                       tokens.len,
                                       &input,
                                       &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&input,
                                              yvex_tokenizer_vocab_size(ctx.tokenizer),
                                              &err);
    }

    printf("token_input: prompt\n");
    printf("model: %s\n", model_arg);
    printf("resolved_path: %s\n", ref.path ? ref.path : "");
    printf("model_input_kind: %s\n",
           ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    printf("tokenizer_status: present\n");
    printf("tokenizer_support: %s\n",
           yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    print_token_input_summary(&input,
                              rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                              input.token_bounds_checked ? "fail" : "not-checked",
                              0ull,
                              input.token_count > 0ull ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    printf("vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    print_token_input_tokens(&input);
    printf("prefill_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");

    yvex_tokens_free(&tokens);
    close_tokenizer_context(&ctx);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_kv(int argc, char **argv)
{
    yvex_kv_shape shape;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    float *append_values = NULL;
    float *read_values = NULL;
    unsigned long long value_count = 0ull;
    unsigned long long append_target = 0ull;
    unsigned long long appended_position = 0ull;
    unsigned long long read_position = 0ull;
    unsigned long long read_checksum = 0ull;
    int append_demo = 0;
    int read_requested = 0;
    int cleanup_attempted = 0;
    const char *cleanup_status = "not-needed";
    int i;
    int rc;

    memset(&shape, 0, sizeof(shape));
    memset(&summary, 0, sizeof(summary));
    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("kv"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.layer_count)) {
                fprintf(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--heads") == 0 || strcmp(argv[i], "--lanes") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.kv_head_count)) {
                fprintf(stderr, "yvex: --heads requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.head_dim)) {
                fprintf(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.capacity)) {
                fprintf(stderr, "yvex: --capacity requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--append-demo") == 0) {
            append_demo = 1;
        } else if (strcmp(argv[i], "--read-position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &read_position)) {
                fprintf(stderr, "yvex: --read-position requires a non-negative integer\n");
                return 2;
            }
            read_requested = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown kv option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help kv' for usage.\n");
            return 2;
        }
    }

    if (shape.layer_count == 0ull || shape.kv_head_count == 0ull ||
        shape.head_dim == 0ull || shape.capacity == 0ull) {
        fprintf(stderr, "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n");
        return 2;
    }

    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    value_count = yvex_kv_cache_position_value_count(kv);
    if (append_demo) {
        append_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!append_values) {
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV append demo buffer\n");
            return 4;
        }
        append_target = shape.capacity > 1ull ? 2ull : 1ull;
        for (i = 0; (unsigned long long)i < append_target; ++i) {
            fill_kv_demo_values(append_values, value_count, (unsigned long long)i);
            rc = yvex_kv_cache_append_position_f32(kv,
                                                   append_values,
                                                   value_count,
                                                   &appended_position,
                                                   &err);
            if (rc != YVEX_OK) {
                free(append_values);
                yvex_kv_cache_close(kv);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    if (read_requested) {
        read_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!read_values) {
            free(append_values);
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV read buffer\n");
            return 4;
        }
        rc = yvex_kv_cache_read_position_f32(kv, read_position, read_values, value_count, &err);
        if (rc != YVEX_OK) {
            free(read_values);
            free(append_values);
            yvex_kv_cache_close(kv);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        read_checksum = checksum_kv_values(read_values, value_count);
    }

    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    if (rc != YVEX_OK) {
        free(read_values);
        free(append_values);
        yvex_kv_cache_close(kv);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    cleanup_attempted = 1;
    yvex_kv_cache_close(kv);
    kv = NULL;
    cleanup_status = "pass";

    printf("kv: ownership\n");
    printf("kv_created: true\n");
    printf("session_owned: %s\n", summary.session_owned ? "true" : "false");
    printf("layers: %llu\n", summary.layer_count);
    printf("heads: %llu\n", summary.kv_head_count);
    printf("head_dim: %llu\n", summary.head_dim);
    printf("capacity: %llu\n", summary.context_length);
    printf("dtype: %s\n", summary.dtype ? summary.dtype : "F32");
    printf("values_per_position: %llu\n", summary.values_per_position);
    printf("bytes_per_position: %llu\n", summary.bytes_per_position);
    printf("planned_bytes: %llu\n", summary.bytes);
    printf("allocated_bytes: %llu\n", summary.allocated_bytes);
    printf("append_count: %llu\n", summary.append_count);
    printf("read_count: %llu\n", summary.read_count);
    printf("written_positions: %llu\n", summary.written_positions);
    printf("last_appended_position: %llu\n", appended_position);
    if (read_requested) {
        unsigned long long sample_count = value_count < 8ull ? value_count : 8ull;
        unsigned long long j;
        printf("read_position: %llu\n", read_position);
        printf("read_value_count: %llu\n", value_count);
        printf("read_checksum: %llu\n", read_checksum);
        printf("read_sample_values:");
        for (j = 0; j < sample_count; ++j) {
            printf("%s%.9g", j == 0 ? " " : ",", (double)read_values[j]);
        }
        printf("\n");
    } else {
        printf("read_position: not-requested\n");
        printf("read_value_count: 0\n");
        printf("read_checksum: 0\n");
    }
    printf("overflow_status: %s\n",
           summary.overflow_status ? summary.overflow_status : "not-overflowed");
    printf("cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", cleanup_status);
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: kv-owned\n");

    free(read_values);
    free(append_values);
    return 0;
}

static int command_detokenize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const char *ids_text = NULL;
    unsigned int *ids = NULL;
    unsigned long long ids_len = 0;
    char out[4096];
    int i;
    int rc;

    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("detokenize"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--ids") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --ids requires a value\n");
                return 2;
            }
            ids_text = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown detokenize option: %s\n", argv[i]);
            return 2;
        }
    }
    if (!ids_text || !parse_id_list(ids_text, &ids, &ids_len)) {
        fprintf(stderr, "yvex: detokenize requires comma-separated --ids\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        free(ids);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_detokenize_ids(ctx.tokenizer, ids, ids_len, out, sizeof(out), &err);
    free(ids);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("text: ");
    print_quoted_bytes(out, (unsigned long long)strlen(out));
    printf("\n");
    printf("status: detokenized\n");

    close_tokenizer_context(&ctx);
    return 0;
}

static int command_engine(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_model_ref model_ref;
    yvex_engine_options options;
    yvex_engine_summary summary;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&options, 0, sizeof(options));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("engine"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a file or alias\n");
                return 2;
            }
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "yvex: unknown engine option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help engine' for usage.\n");
            return 2;
        } else {
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[i];
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: engine requires FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n");
        return 2;
    }
    if (backend_name && strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (backend_name) {
        rc = enforce_registered_identity_cli(&model_ref, "engine");
        if (rc != YVEX_OK) {
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }
    }

    options.model_path = model_ref.path;
    options.load_tokenizer = backend_name ? 0 : 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    options.attach_weights = backend_name != NULL;
    options.backend_name = backend_name;
    options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: engine-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, &err);
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine status: %s\n", yvex_engine_status_name(summary.status));
    printf("format: gguf\n");
    printf("architecture: %s\n", summary.architecture);
    printf("model_name: %s\n", summary.model_name);
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("known_tensor_bytes: %llu\n", summary.known_tensor_bytes);
    printf("unsupported_tensor_accounting: %llu\n", summary.unsupported_tensor_accounting);
    printf("tokenizer_model: %s\n", summary.tokenizer_model);
    printf("tokenizer_support: %s\n", summary.tokenizer_support);
    printf("graph_status: %s\n", summary.graph_status);
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("weight_backend_allocated_bytes: %llu\n", summary.weight_backend_allocated_bytes);
    if (summary.weights_attached) {
        const yvex_tensor_table *table = yvex_engine_tensors(engine);
        unsigned long long count = yvex_tensor_table_count(table);
        unsigned long long j;

        for (j = 0; j < count; ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(table, j);
            unsigned int d;

            if (!tensor) {
                continue;
            }
            printf("attached_weight_%llu: %s role=%s rank=%u dims=[",
                   j,
                   tensor->name ? tensor->name : "",
                   yvex_tensor_role_name(tensor->role),
                   tensor->rank);
            for (d = 0; d < tensor->rank; ++d) {
                if (d > 0) {
                    printf(",");
                }
                printf("%llu", tensor->dims[d]);
            }
            printf("] dtype=%s bytes=%llu\n",
                   yvex_dtype_name(tensor->dtype),
                   tensor->storage_bytes);
        }
    }
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_engine_diagnostic_reason(engine));
    printf("status: %s\n", summary.weights_attached ? "engine-weights-attached" : "engine-descriptor");

    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static int command_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("name: YVEX\n");
    printf("version: %s\n", yvex_version_string());
    printf("language: C\n");
    printf("interface: CLI-only\n");
    printf("status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token input boundary, prefill state foundation, minimal KV binding, and minimal KV ownership\n");
    printf("library: libyvex.a\n");
    printf("filesystem: implemented\n");
    printf("artifact: open/read implemented\n");
    printf("gguf: metadata/tensor directory parsing implemented\n");
    printf("model: descriptor-only implemented\n");
    printf("tokenizer: fixture encode/decode implemented\n");
    printf("token_input: explicit token boundary implemented\n");
    printf("prefill_state: segment-summary foundation and minimal KV binding implemented\n");
    printf("prompt: default renderer implemented\n");
    printf("graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward primitive implemented\n");
    printf("planner: estimate-only implemented\n");
    printf("backend: CPU reference implemented\n");
    printf("backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is available\n");
    printf("weights: selected tensor materialization implemented\n");
    printf("engine: descriptor open and selected-weight attachment implemented\n");
    printf("session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented\n");
    printf("run: accepted-only runtime shell implemented\n");
    printf("chat: accepted-only REPL shell implemented\n");
    printf("metrics: runtime collector implemented\n");
    printf("trace: JSONL writer implemented\n");
    printf("profile: JSON writer implemented\n");
    printf("run_artifacts: metrics/trace/profile files implemented\n");
    printf("source_manifest: provenance JSON writer implemented\n");
    printf("native_weights: safetensors header inventory implemented\n");
    printf("gguf_template: contract validator implemented\n");
    printf("gguf_emit: controlled GGUF writer implemented\n");
    printf("conversion: open-weight selected tensor bridge implemented\n");
    printf("model_ref: alias-or-path resolver implemented\n");
    printf("model_registry: local model alias registry implemented\n");
    printf("quant_job: external quantization job manifest implemented\n");
    printf("qtype_support: conversion support matrix implemented\n");
    printf("weight_mapping: tensor adapter contract implemented\n");
    printf("quant_policy: manifest validator implemented\n");
    printf("imatrix: calibration artifact manifest implemented\n");
    printf("server_binary: yvexd shell implemented\n");
    printf("server_endpoints: health/metrics/models status implemented\n");
    printf("server_generation: not implemented\n");
    printf("kv: minimal session-owned append/read boundary implemented\n");
    printf("logits: unavailable skeleton implemented\n");
    printf("generation: unsupported\n");
    printf("inference: not implemented\n");
    printf("cuda: available when local driver/device probe succeeds\n");
    printf("server: yvexd status shell implemented\n");
    return 0;
}

static int command_paths(int argc, char **argv)
{
    const char *project_root = NULL;
    int want_run = 0;
    int want_create = 0;
    int i;
    int rc;
    yvex_paths paths;
    yvex_run_dir run;
    yvex_error err;

    yvex_error_clear(&err);

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--project") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --project requires a path\n");
                return 2;
            }
            project_root = argv[++i];
        } else if (strcmp(argv[i], "--run") == 0) {
            want_run = 1;
        } else if (strcmp(argv[i], "--create") == 0) {
            want_create = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_cli_print_command_help(stdout, yvex_cli_find_command("paths"));
            return 0;
        } else {
            fprintf(stderr, "yvex: unknown paths option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help paths' for usage.\n");
            return 2;
        }
    }

    if (want_create && !want_run) {
        fprintf(stderr, "yvex: --create requires --run\n");
        return 2;
    }

    rc = project_root ? yvex_paths_project(&paths, project_root, &err) : yvex_paths_default(&paths, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (!want_run) {
        rc = yvex_paths_print(&paths, stdout, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        return 0;
    }

    rc = yvex_run_dir_prepare(&run, &paths, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (want_create) {
        rc = yvex_run_dir_create(&run, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
    }

    rc = yvex_run_dir_print(&run, stdout, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }
    return 0;
}

static void print_prefill_state_summary(const yvex_prefill_state_summary *summary,
                                        const char *model_arg,
                                        const char *backend_name,
                                        const char *token_input_status,
                                        const char *status)
{
    unsigned long long i;

    printf("prefill: state\n");
    printf("prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    printf("prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "segment-summary");
    printf("sequence_execution_mode: %s\n",
           summary && summary->sequence_execution_mode
               ? summary->sequence_execution_mode
               : "independent-token-segments");
    printf("prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "preflight");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n",
           summary && summary->backend_name && strcmp(summary->backend_name, "none") != 0
               ? summary->backend_name
               : (backend_name ? backend_name : "cpu"));
    printf("segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    printf("token_count: %llu\n", summary ? summary->token_count : 0ull);
    printf("tokens_processed: %llu\n", summary ? summary->tokens_processed : 0ull);
    printf("position_start: %llu\n", summary ? summary->position_start : 0ull);
    printf("position_end: %llu\n", summary ? summary->position_end : 0ull);
    printf("failed_token_index: %llu\n", summary ? summary->failed_token_index : 0ull);
    printf("segment_graph_executions: %llu\n", summary ? summary->segment_graph_executions : 0ull);
    printf("segment_output_count: %llu\n", summary ? summary->segment_output_count : 0ull);
    printf("segment_output_bytes: %llu\n", summary ? summary->segment_output_bytes : 0ull);
    printf("prefill_aggregate_checksum: %llu\n", summary ? summary->aggregate_checksum : 0ull);
    printf("prefill_final_token_checksum: %llu\n", summary ? summary->final_token_checksum : 0ull);
    printf("prefill_total_output_bytes: %llu\n", summary ? summary->total_output_bytes : 0ull);
    printf("prefill_scratch_bytes: %llu\n", summary ? summary->scratch_bytes : 0ull);
    printf("prefill_max_abs_diff: %.9g\n", summary ? summary->max_abs_diff : 0.0);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    if (summary && summary->cuda_parity) {
        printf("prefill_cuda_parity: pass\n");
    }
    printf("kv_ready: %s\n", summary && summary->kv_ready ? "true" : "false");
    printf("session_kv_owned: %s\n",
           summary && summary->session_kv_owned ? "true" : "false");
    printf("kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    printf("kv_binding_kind: %s\n",
           summary && summary->kv_binding_kind ? summary->kv_binding_kind : "none");
    printf("kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    printf("kv_owner: %s\n",
           summary && summary->kv_owner ? summary->kv_owner : "none");
    printf("kv_dtype: %s\n",
           summary && summary->kv_dtype ? summary->kv_dtype : "none");
    printf("kv_layers: %llu\n", summary ? summary->kv_layers : 0ull);
    printf("kv_heads: %llu\n", summary ? summary->kv_heads : 0ull);
    printf("kv_head_dim: %llu\n", summary ? summary->kv_head_dim : 0ull);
    printf("kv_capacity: %llu\n", summary ? summary->kv_capacity : 0ull);
    printf("kv_values_per_position: %llu\n", summary ? summary->kv_values_per_position : 0ull);
    printf("kv_bytes_per_position: %llu\n", summary ? summary->kv_bytes_per_position : 0ull);
    printf("kv_planned_bytes: %llu\n", summary ? summary->kv_planned_bytes : 0ull);
    printf("kv_allocated_bytes: %llu\n", summary ? summary->kv_allocated_bytes : 0ull);
    printf("kv_positions_written: %llu\n", summary ? summary->kv_positions_written : 0ull);
    printf("kv_append_count: %llu\n", summary ? summary->kv_append_count : 0ull);
    printf("kv_read_count: %llu\n", summary ? summary->kv_read_count : 0ull);
    printf("kv_read_position: %llu\n", summary ? summary->kv_read_position : 0ull);
    printf("kv_read_value_count: %llu\n", summary ? summary->kv_read_value_count : 0ull);
    printf("kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    printf("kv_read_sample_values:");
    if (summary && summary->kv_read_sample_count > 0ull) {
        for (i = 0; i < summary->kv_read_sample_count; ++i) {
            printf("%s%.9g", i == 0 ? " " : ",", (double)summary->kv_read_sample_values[i]);
        }
    }
    printf("\n");
    printf("kv_overflow: %s\n",
           summary && summary->kv_overflow_status ? summary->kv_overflow_status : "not-checked");
    printf("kv_cleanup_status: %s\n",
           summary && summary->kv_cleanup_status ? summary->kv_cleanup_status : "not-needed");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", status ? status : "prefill-state-fail");
}

static void init_prefill_summary_cli_defaults(yvex_prefill_state_summary *summary,
                                              const char *segment_name,
                                              int attach_kv,
                                              const yvex_kv_shape *shape)
{
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->prefill_state_kind = "segment-summary";
    summary->sequence_execution_mode = "independent-token-segments";
    summary->prefill_phase = "preflight";
    summary->segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    summary->cleanup_status = "not-needed";
    summary->generation_status = "unsupported";
    summary->kv_binding_kind = attach_kv ? "minimal-diagnostic" : "none";
    summary->kv_status = attach_kv ? "planned" : "not-requested";
    summary->kv_owner = "none";
    summary->kv_dtype = "none";
    summary->kv_overflow_status = "not-checked";
    summary->kv_cleanup_status = "not-needed";
    if (shape) {
        summary->kv_layers = shape->layer_count;
        summary->kv_heads = shape->kv_head_count;
        summary->kv_head_dim = shape->head_dim;
        summary->kv_capacity = shape->capacity;
    }
}

static int command_prefill(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    yvex_kv_shape kv_shape;
    unsigned long long vocab_size = 0ull;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("prefill"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.layer_count)) {
                fprintf(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.kv_head_count)) {
                fprintf(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.head_dim)) {
                fprintf(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.capacity)) {
                fprintf(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            fprintf(stderr, "yvex: unknown prefill option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help prefill' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !tokens_text || !segment_name) {
        fprintf(stderr, "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported prefill segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        fprintf(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        fprintf(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "prefill");
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = preflight_graph_guard(&model_ref,
                               backend_name,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        print_graph_guard_report(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    prefill_options.token_input = &token_input;
    prefill_options.segment_name = segment_name;
    prefill_options.position_start = 0ull;
    prefill_options.attach_kv = attach_kv;
    prefill_options.kv_shape = kv_shape;
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, &err);
    if (rc != YVEX_OK) {
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static int command_plan(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.backend_name = "cpu";

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("plan"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                fprintf(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            options.backend_name = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown plan option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help plan' for usage.\n");
            return 2;
        }
    }

    rc = open_model_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_dump(plan, stdout, &err);
    }

    yvex_plan_close(plan);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_prompt(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_prompt_message messages[16];
    unsigned long long message_count = 0;
    yvex_prompt_options options;
    yvex_rendered_prompt rendered;
    yvex_tokens tokens;
    yvex_error err;
    const char *chat_template = NULL;
    unsigned long long chat_template_len = 0;
    int want_tokens = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&rendered, 0, sizeof(rendered));
    options.add_bos = 0;
    options.add_eos = 0;
    options.add_generation_prompt = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("prompt"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        yvex_prompt_role role;
        if (strcmp(argv[i], "--system") == 0) {
            role = YVEX_PROMPT_ROLE_SYSTEM;
        } else if (strcmp(argv[i], "--user") == 0) {
            role = YVEX_PROMPT_ROLE_USER;
        } else if (strcmp(argv[i], "--assistant") == 0) {
            role = YVEX_PROMPT_ROLE_ASSISTANT;
        } else if (strcmp(argv[i], "--no-generation-prompt") == 0) {
            options.add_generation_prompt = 0;
            continue;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            want_tokens = 1;
            continue;
        } else {
            fprintf(stderr, "yvex: unknown prompt option: %s\n", argv[i]);
            return 2;
        }

        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: prompt option %s requires text\n", argv[i]);
            return 2;
        }
        if (message_count >= sizeof(messages) / sizeof(messages[0])) {
            fprintf(stderr, "yvex: too many prompt messages\n");
            return 2;
        }
        messages[message_count].role = role;
        messages[message_count].content = argv[++i];
        message_count += 1;
    }

    if (message_count == 0) {
        fprintf(stderr, "yvex: prompt requires at least one message\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_prompt_render(&rendered, ctx.tokenizer, messages, message_count, &options, &err);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("template: yvex-default\n");
    printf("chat_template_metadata: %s\n",
           yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK
               ? "present-unsupported"
               : "absent");
    printf("rendered_bytes: %llu\n", rendered.len);
    printf("rendered:\n%s", rendered.text);

    if (want_tokens) {
        rc = yvex_tokenize_text(ctx.tokenizer, rendered.text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_rendered_prompt_free(&rendered);
            close_tokenizer_context(&ctx);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("tokens: %llu\n", tokens.len);
        print_token_ids(&tokens);
        yvex_tokens_free(&tokens);
    }
    printf("status: rendered\n");

    yvex_rendered_prompt_free(&rendered);
    close_tokenizer_context(&ctx);
    return 0;
}

static void trim_line(char *line)
{
    size_t len;

    if (!line) {
        return;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
        line[len - 1u] = '\0';
        len -= 1u;
    }
}

static void cli_copy_text(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src ? src : "");
    dst[cap - 1u] = '\0';
}

static void cli_set_result_artifacts(yvex_chat_accept_result *result,
                                     const yvex_run_artifacts *artifacts)
{
    if (!result || !artifacts) {
        return;
    }
    cli_copy_text(result->run_id, sizeof(result->run_id), artifacts->run_id);
    cli_copy_text(result->run_dir, sizeof(result->run_dir), artifacts->run_dir);
    if (artifacts->has_metrics) {
        cli_copy_text(result->metrics_out, sizeof(result->metrics_out), artifacts->metrics_path);
    }
    if (artifacts->has_trace) {
        cli_copy_text(result->trace_out, sizeof(result->trace_out), artifacts->trace_path);
    }
    if (artifacts->has_profile) {
        cli_copy_text(result->profile_out, sizeof(result->profile_out), artifacts->profile_path);
    }
}

static int cli_write_observability_files(const char *command_name,
                                         const char *model_name,
                                         const char *backend_name,
                                         const char *status,
                                         const yvex_run_artifacts *artifacts,
                                         const yvex_metrics *metrics,
                                         int argc,
                                         char **argv,
                                         yvex_error *err)
{
    yvex_profile_summary profile;
    yvex_metric_counters counters;
    int rc;

    if (!artifacts || !metrics) {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_run_artifacts_write_command(artifacts, argc, argv, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (artifacts->has_metrics) {
        rc = yvex_metrics_write_json(artifacts->metrics_path, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (artifacts->has_profile) {
        rc = yvex_metrics_get_counters(metrics, &counters, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        memset(&profile, 0, sizeof(profile));
        profile.run_id = artifacts->run_id;
        profile.command = command_name;
        profile.model_name = model_name;
        profile.backend_name = backend_name;
        profile.status = status;
        profile.execution_ready = 0;
        profile.counters = counters;
        rc = yvex_profile_write_json(artifacts->profile_path, &profile, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

static int command_run(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_chat_accept_result result;
    yvex_engine_summary engine_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *prompt_text = NULL;
    const char *system_text = NULL;
    const char *output = "plain";
    const char *status_line = "off";
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    int save_run = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("run"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --prompt requires a value\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else if (strcmp(argv[i], "--system") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --system requires a value\n");
                return 2;
            }
            system_text = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --output requires plain or json\n");
                return 2;
            }
            output = argv[++i];
            if (strcmp(output, "plain") != 0 && strcmp(output, "json") != 0) {
                fprintf(stderr, "yvex: --output must be plain or json\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--status-line") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
            status_line = argv[++i];
            if (strcmp(status_line, "auto") != 0 && strcmp(status_line, "off") != 0 &&
                strcmp(status_line, "always") != 0) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown run option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help run' for usage.\n");
            return 2;
        }
    }

    if (!model_path) {
        fprintf(stderr, "yvex: --model is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!prompt_text) {
        fprintf(stderr, "yvex: --prompt is required for yvex run in diagnostic runtime\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("run status: backend-unsupported\n");
        printf("backend: cuda\n");
        printf("execution_ready: false\n");
        printf("reason: %s\n", yvex_error_message(&err));
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    if (yvex_engine_get_summary(runtime.engine, &engine_summary, &err) == YVEX_OK) {
        (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                           engine_summary.unsupported_tensor_accounting, &err);
    }

    rc = yvex_chat_runtime_accept_user_text(&runtime, system_text, prompt_text, &result, &err);
    if (rc != YVEX_OK) {
        yvex_chat_runtime_close(&runtime);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "accepted-only",
                          "decode runtime is not implemented in diagnostic runtime", &err);
    cli_set_result_artifacts(&result, &artifacts);

    if (strcmp(status_line, "always") == 0) {
        (void)yvex_status_line_print(stderr, "accept", result.prompt_tokens, result.position);
    }

    if (strcmp(output, "json") == 0) {
        (void)yvex_run_command_json(stdout, &result);
    } else {
        (void)yvex_run_command_plain(stdout, &result);
    }

    yvex_trace_close(trace);
    rc = cli_write_observability_files("run", result.model_name, result.backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

static int command_session(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_model_ref model_ref;
    yvex_engine_options engine_options;
    yvex_session_options session_options;
    yvex_session_summary summary;
    yvex_backend_options backend_options;
    yvex_tokens tokens;
    yvex_error err;
    const char *backend_name = "cpu";
    const char *text = NULL;
    int accept_tokens = 0;
    int tokenized = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&session_options, 0, sizeof(session_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&tokens, 0, sizeof(tokens));
    memset(&model_ref, 0, sizeof(model_ref));
    session_options.allow_partial_graph = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("session"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &session_options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--text") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = argv[++i];
        } else if (strcmp(argv[i], "--accept-tokens") == 0) {
            accept_tokens = 1;
        } else {
            fprintf(stderr, "yvex: unknown session option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help session' for usage.\n");
            return 2;
        }
    }

    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, argv[2], NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "session");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = text ? 1 : 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        yvex_engine_close(engine);
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (text) {
        rc = yvex_tokenize_text(yvex_engine_tokenizer(engine), text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_session_close(session);
            yvex_backend_close(backend);
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        tokenized = 1;
        if (accept_tokens) {
            rc = yvex_session_accept_tokens(session, &tokens, &err);
            if (rc != YVEX_OK) {
                yvex_tokens_free(&tokens);
                yvex_session_close(session);
                yvex_backend_close(backend);
                yvex_engine_close(engine);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    rc = yvex_session_get_summary(session, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_tokens_free(&tokens);
        yvex_session_close(session);
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine_status: %s\n", summary.engine_status);
    printf("backend: %s\n", summary.backend_kind);
    printf("backend_status: %s\n", summary.backend_status);
    printf("session_state: %s\n", yvex_session_state_name(summary.state));
    printf("context_length: %llu\n", summary.context_length);
    printf("position: %llu\n", summary.position);
    printf("accepted_tokens: %llu\n", summary.accepted_tokens);
    printf("kv_status: %s\n", summary.kv_status);
    printf("kv_bytes: %llu\n", summary.kv_bytes);
    printf("logits_status: %s\n", summary.logits_status);
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        printf("tokens: %llu\n", tokens.len);
    }
    printf("status: %s\n", accept_tokens ? "session-token-accepted" : "session-created");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static void print_chat_slash_help(FILE *fp)
{
    fprintf(fp, "commands:\n");
    fprintf(fp, "  /help\n");
    fprintf(fp, "  /status\n");
    fprintf(fp, "  /model\n");
    fprintf(fp, "  /backend\n");
    fprintf(fp, "  /tokens\n");
    fprintf(fp, "  /reset\n");
    fprintf(fp, "  /quit\n");
}

static void print_chat_model(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_engine_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_engine_get_summary(runtime->engine, &summary, &err) == YVEX_OK) {
        fprintf(fp, "model: %s\n", summary.model_name);
        fprintf(fp, "architecture: %s\n", summary.architecture);
        fprintf(fp, "tokenizer: %s\n", summary.tokenizer_model);
    }
}

static void print_chat_backend(FILE *fp, const yvex_chat_runtime *runtime)
{
    fprintf(fp, "backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(runtime->backend)));
    fprintf(fp, "status: %s\n", yvex_backend_status_name(yvex_backend_status_of(runtime->backend)));
    fprintf(fp, "capabilities:\n");
    fprintf(fp, "  tensor_alloc: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ? "yes" : "no");
    fprintf(fp, "  tensor_read_write: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE) ? "yes" : "no");
    fprintf(fp, "  op_embed: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_EMBED) ? "yes" : "no");
    fprintf(fp, "  op_mlp: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_MLP) ? "yes" : "no");
    fprintf(fp, "  op_rope: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_ROPE) ? "yes" : "no");
}

static void print_chat_tokens(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_session_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_chat_runtime_get_summary(runtime, &summary, &err) == YVEX_OK) {
        fprintf(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
        fprintf(fp, "position: %llu\n", summary.position);
    }
}

static int handle_chat_slash(yvex_chat_runtime *runtime,
                             yvex_slash_command command,
                             const char *line,
                             int *done,
                             yvex_error *err)
{
    int rc;

    switch (command) {
    case YVEX_SLASH_HELP:
        print_chat_slash_help(stdout);
        return YVEX_OK;
    case YVEX_SLASH_STATUS:
        return yvex_chat_runtime_print_status(stdout, runtime, err);
    case YVEX_SLASH_MODEL:
        print_chat_model(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_BACKEND:
        print_chat_backend(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_TOKENS:
        print_chat_tokens(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_RESET:
        rc = yvex_chat_runtime_reset(runtime, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        printf("session reset\n");
        printf("position: 0\n");
        return YVEX_OK;
    case YVEX_SLASH_QUIT:
        printf("bye\n");
        *done = 1;
        return YVEX_OK;
    case YVEX_SLASH_UNKNOWN:
        printf("unknown slash command: %s\n", line ? line : "");
        printf("type /help\n");
        return YVEX_OK;
    case YVEX_SLASH_NOT_COMMAND:
        break;
    }
    return YVEX_OK;
}

static int resolve_chat_model_ref(yvex_model_ref *out,
                                  const char *explicit_model,
                                  yvex_error *err)
{
    yvex_model_registry *registry = NULL;
    const yvex_model_registry_entry *selected;
    char alias[256];
    int n;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat", "model reference output is required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (explicit_model && explicit_model[0]) {
        return yvex_model_ref_resolve(out, explicit_model, NULL, err);
    }

    rc = models_registry_open(&registry, NULL, 0, err);
    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_IO) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                           "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
            return YVEX_ERR_INVALID_ARG;
        }
        return rc;
    }

    selected = yvex_model_registry_selected(registry);
    if (!selected || !selected->alias || !selected->alias[0]) {
        yvex_model_registry_close(registry);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                       "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(alias, sizeof(alias), "%s", selected->alias);
    yvex_model_registry_close(registry);
    if (n < 0 || (size_t)n >= sizeof(alias)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "chat", "selected model alias is too long");
        return YVEX_ERR_BOUNDS;
    }

    return yvex_model_ref_resolve(out, alias, NULL, err);
}

static int command_chat(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_engine_summary engine_summary;
    yvex_session_summary session_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    char line[4096];
    int save_run = 0;
    int done = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("chat"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown chat option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help chat' for usage.\n");
            return 2;
        }
    }

    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex chat in diagnostic runtime\n");
        return 2;
    }

    rc = resolve_chat_model_ref(&model_ref, model_path, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "chat", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: chat-backend-unsupported\n");
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    (void)yvex_engine_get_summary(runtime.engine, &engine_summary, &err);
    (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                       engine_summary.unsupported_tensor_accounting, &err);
    (void)yvex_chat_runtime_get_summary(&runtime, &session_summary, &err);
    printf("YVEX chat runtime\n");
    printf("model: %s\n", engine_summary.model_name);
    printf("backend: %s\n", backend_name);
    printf("session_state: %s\n", yvex_session_state_name(session_summary.state));
    printf("generation: unsupported in diagnostic runtime\n");
    printf("type /help for commands\n");

    while (!done) {
        printf("yvex> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '/') {
            rc = handle_chat_slash(&runtime, yvex_slash_parse(line), line, &done, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            continue;
        }

        {
            yvex_chat_accept_result result;
            rc = yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_CHAT_TURN, &phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            rc = yvex_chat_runtime_accept_user_text(&runtime, NULL, line, &result, &err);
            (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_CHAT_TURN, phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            (void)yvex_metrics_add_chat_turn(metrics, &err);
            (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_CHAT_TURN, "chat_turn", "accepted",
                                  "user prompt accepted", &err);
            printf("accepted tokens: %llu\n", result.prompt_tokens);
            printf("position: %llu\n", result.position);
            printf("assistant: [generation unsupported in diagnostic runtime]\n");
        }
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "accepted-only",
                          "chat runtime exited without generation", &err);
    yvex_trace_close(trace);
    rc = cli_write_observability_files("chat", engine_summary.model_name, backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

int yvex_cli_command_backend(int argc, char **argv)
{
    return command_backend(argc, argv);
}

int yvex_cli_command_chat(int argc, char **argv)
{
    return command_chat(argc, argv);
}

int yvex_cli_command_cuda_info(int argc, char **argv)
{
    return command_cuda_info(argc, argv);
}

int yvex_cli_command_detokenize(int argc, char **argv)
{
    return command_detokenize(argc, argv);
}

int yvex_cli_command_engine(int argc, char **argv)
{
    return command_engine(argc, argv);
}

int yvex_cli_command_info(int argc, char **argv)
{
    return command_info(argc, argv);
}

int yvex_cli_command_input(int argc, char **argv)
{
    return command_input(argc, argv);
}

int yvex_cli_command_kv(int argc, char **argv)
{
    return command_kv(argc, argv);
}

int yvex_cli_command_paths(int argc, char **argv)
{
    return command_paths(argc, argv);
}

int yvex_cli_command_plan(int argc, char **argv)
{
    return command_plan(argc, argv);
}

int yvex_cli_command_prefill(int argc, char **argv)
{
    return command_prefill(argc, argv);
}

int yvex_cli_command_prompt(int argc, char **argv)
{
    return command_prompt(argc, argv);
}

int yvex_cli_command_run(int argc, char **argv)
{
    return command_run(argc, argv);
}

int yvex_cli_command_session(int argc, char **argv)
{
    return command_session(argc, argv);
}

int yvex_cli_command_tokenize(int argc, char **argv)
{
    return command_tokenize(argc, argv);
}

int yvex_cli_command_tokenizer(int argc, char **argv)
{
    return command_tokenizer(argc, argv);
}
