/* Owner: CLI tokenizer command.
 * Owns: tokenizer argv validation, dispatch, help, and compatibility rendering.
 * Does not own: tokenizer parsing, prompt semantics, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed tokenizer APIs and returns process exit status.
 * Purpose: provide tokenizer argv validation, dispatch, help, and compatibility rendering.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/io/private.h"
#include <yvex/core.h>
#include <yvex/model.h>
#include <yvex/tokenizer.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const literal_pair_0[] = { "yvex: input requires tokens or prompt",
    "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT"
};

static const char *const literal_pair_1[] = { "token_input_status: fail",
    "token_input_kind: prompt-text"};

static const char *const literal_pair_2[] = { "prefill_ready: false",
    "status: token-input-fail"};

static const char *const literal_pair_3[] = { "",
    "status: detokenized"};

static const char *const literal_lines_0[] = { "prefill_ready: false",
    "logits_ready: false",
    "generation: unsupported"};

static const char *const literal_lines_1[] = { "token_bounds_status: not-checked",
    "prefill_ready: false",
    "logits_ready: false",
    "generation: unsupported",
    "status: token-input-fail"};

static const char *const literal_lines_2[] = { "prefill_ready: false",
    "logits_ready: false",
    "generation: unsupported"};

static const char *const literal_lines_3[] = { "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS",
    "       yvex input prompt --model FILE_OR_ALIAS --text TEXT",
    "\nInput parses explicit tokens or tokenizer-backed prompt text into validated token input."};

/* Domain-owned command surface moved out of core.c. */

/* Purpose: Render print special id line from typed facts (`print_special_id_line`). */
static int print_special_id_line(const char *name, int (*fn)(const yvex_tokenizer *, unsigned int *),
    const yvex_tokenizer *tokenizer)
{
    unsigned int id;
    int rc = fn(tokenizer, &id);

    if (rc == YVEX_OK) {
        yvex_cli_out_writef(stdout, "%s: %u\n", name, id);
    } else {
        yvex_cli_out_writef(stdout, "%s: absent\n", name);
    }
    return YVEX_OK;
}

/* Purpose: Orchestrate the typed command tokenizer request (`command_tokenizer`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_tokenizer(int arg_count, char **args)
{
    yvex_model_context ctx;
    yvex_error err;
    const char *chat_template;
    unsigned long long chat_template_len;
    int rc;

    yvex_error_clear(&err);

    if (arg_count != 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        if (arg_count == 3) {
            yvex_tokenizer_help(stdout);
            return 0;
        }
        yvex_cli_out_writef(stderr, "yvex: tokenizer requires exactly one path\n");
        yvex_cli_out_writef(stderr, "usage: yvex tokenizer <path>\n");
        return 2;
    }

    rc = yvex_model_context_open_tokenizer(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "format: gguf\n");
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(yvex_model_arch(ctx.model)));
    yvex_cli_out_writef(stdout, "model_name: %s\n", yvex_model_name(ctx.model));
    yvex_cli_out_writef(stdout, "tokenizer_model: %s\n",
        yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(ctx.tokenizer)));
    yvex_cli_out_writef(stdout, "support: %s\n", yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    yvex_cli_out_writef(stdout, "vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    (void)print_special_id_line("bos_token_id", yvex_tokenizer_bos_id, ctx.tokenizer);
    (void)print_special_id_line("eos_token_id", yvex_tokenizer_eos_id, ctx.tokenizer);
    (void)print_special_id_line("unk_token_id", yvex_tokenizer_unk_id, ctx.tokenizer);
    if (yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK) {
        (void)chat_template;
        yvex_cli_out_writef(stdout, "chat_template: present-unsupported\n");
    } else {
        yvex_cli_out_writef(stdout, "chat_template: absent\n");
    }
    yvex_cli_out_writef(stdout, "status: tokenizer-descriptor\n");

    yvex_model_context_close(&ctx);
    return 0;
}

/* Purpose: Orchestrate the typed command tokenize request (`command_tokenize`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_tokenize(int arg_count, char **args)
{
    yvex_model_context ctx;
    yvex_tokens tokens;
    yvex_error err;
    const char *text = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_tokenize_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--text") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = args[++i];
        } else if (strcmp(args[i], "--pieces") == 0 || strcmp(args[i], "--no-bos") == 0 || strcmp(args[i],
            "--eos") == 0) {
            /* Accepted for tokenizer layer CLI shape; fixture tokenization has no implicit BOS/EOS. */
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown tokenize option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help tokenize' for usage.\n");
            return 2;
        }
    }
    if (!text) {
        yvex_cli_out_writef(stderr, "yvex: tokenize requires --text\n");
        return 2;
    }

    rc = yvex_model_context_open_tokenizer(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, text, &tokens, &err);
    if (rc != YVEX_OK) {
        yvex_model_context_close(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "tokens: %llu\n", tokens.len);
    print_token_ids(&tokens);
    yvex_cli_out_writef(stdout, "pieces:\n");
    for (i = 0; (unsigned long long)i < tokens.len; ++i) {
        const yvex_token_info *token = yvex_tokenizer_token_at(ctx.tokenizer, tokens.ids[i]);
        yvex_cli_out_writef(stdout, "  %u ", tokens.ids[i]);
        print_quoted_bytes(token ? token->text : "", token ? token->text_len : 0);
        yvex_cli_out_writef(stdout, "\n");
    }
    yvex_cli_out_writef(stdout, "status: tokenized\n");

    yvex_tokens_free(&tokens);
    yvex_model_context_close(&ctx);
    return 0;
}

/* Purpose: Orchestrate the typed command detokenize request (`command_detokenize`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_detokenize(int arg_count, char **args)
{
    yvex_model_context ctx;
    yvex_error err;
    const char *ids_text = NULL;
    unsigned int *ids = NULL;
    unsigned long long ids_len = 0;
    char out[4096];
    int i;
    int rc;

    yvex_error_clear(&err);

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_detokenize_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--ids") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --ids requires a value\n");
                return 2;
            }
            ids_text = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown detokenize option: %s\n", args[i]);
            return 2;
        }
    }
    if (!ids_text || !parse_id_list(ids_text, &ids, &ids_len)) {
        yvex_cli_out_writef(stderr, "yvex: detokenize requires comma-separated --ids\n");
        return 2;
    }

    rc = yvex_model_context_open_tokenizer(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        free(ids);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_detokenize_ids(ctx.tokenizer, ids, ids_len, out, sizeof(out), &err);
    free(ids);
    if (rc != YVEX_OK) {
        yvex_model_context_close(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "text: ");
    print_quoted_bytes(out, (unsigned long long)strlen(out));
    yvex_cli_out_lines(stdout, literal_pair_3, sizeof(literal_pair_3) / sizeof(literal_pair_3[0]));

    yvex_model_context_close(&ctx);
    return 0;
}

/* Purpose: Orchestrate the typed command prompt request (`command_prompt`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_prompt(int arg_count, char **args)
{
    yvex_model_context ctx;
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

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_prompt_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 3; i < arg_count; ++i) {
        yvex_prompt_role role;
        if (strcmp(args[i], "--system") == 0) {
            role = YVEX_PROMPT_ROLE_SYSTEM;
        } else if (strcmp(args[i], "--user") == 0) {
            role = YVEX_PROMPT_ROLE_USER;
        } else if (strcmp(args[i], "--assistant") == 0) {
            role = YVEX_PROMPT_ROLE_ASSISTANT;
        } else if (strcmp(args[i], "--no-generation-prompt") == 0) {
            options.add_generation_prompt = 0;
            continue;
        } else if (strcmp(args[i], "--tokens") == 0) {
            want_tokens = 1;
            continue;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown prompt option: %s\n", args[i]);
            return 2;
        }

        if (i + 1 >= arg_count) {
            yvex_cli_out_writef(stderr, "yvex: prompt option %s requires text\n", args[i]);
            return 2;
        }
        if (message_count >= sizeof(messages) / sizeof(messages[0])) {
            yvex_cli_out_writef(stderr, "yvex: too many prompt messages\n");
            return 2;
        }
        messages[message_count].role = role;
        messages[message_count].content = args[++i];
        message_count += 1;
    }

    if (message_count == 0) {
        yvex_cli_out_writef(stderr, "yvex: prompt requires at least one message\n");
        return 2;
    }

    rc = yvex_model_context_open_tokenizer(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_prompt_render(&rendered, ctx.tokenizer, messages, message_count, &options, &err);
    if (rc != YVEX_OK) {
        yvex_model_context_close(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "template: yvex-default\n");
    yvex_cli_out_writef(stdout, "chat_template_metadata: %s\n",
           yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK
               ? "present-unsupported"
               : "absent");
    yvex_cli_out_writef(stdout, "rendered_bytes: %llu\n", rendered.len);
    yvex_cli_out_writef(stdout, "rendered:\n%s", rendered.text);

    if (want_tokens) {
        rc = yvex_tokenize_text(ctx.tokenizer, rendered.text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_rendered_prompt_free(&rendered);
            yvex_model_context_close(&ctx);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        yvex_cli_out_writef(stdout, "tokens: %llu\n", tokens.len);
        print_token_ids(&tokens);
        yvex_tokens_free(&tokens);
    }
    yvex_cli_out_writef(stdout, "status: rendered\n");

    yvex_rendered_prompt_free(&rendered);
    yvex_model_context_close(&ctx);
    return 0;
}

/* Render the explicit tokens attached to one admitted input request. */
/* Purpose: Render print token input tokens from typed facts (`print_token_input_tokens`). */
static void print_token_input_tokens(const yvex_token_input *input)
{
    unsigned long long i;

    for (i = 0; input && i < input->token_count; ++i) {
        yvex_cli_out_writef(stdout, "token_%llu: %u\n", i, input->tokens[i]);
    }
}

/* Render the stable token-input summary shared by input and prefill commands. */
/* Purpose: Render print token input summary from typed facts (`print_token_input_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_token_input_summary(const yvex_token_input *input,
                               const char *status,
                               const char *bounds_status,
                               unsigned long long selected_index,
                               unsigned int selected_token,
                               int has_selected)
{
    yvex_cli_out_writef(stdout, "token_input_status: %s\n", status ? status : "fail");
    yvex_cli_out_writef(stdout, "token_input_kind: %s\n",
                        input ? yvex_token_input_kind_name(input->kind) : "unknown");
    yvex_cli_out_writef(stdout, "token_count: %llu\n", input ? input->token_count : 0ull);
    if (input) {
        yvex_cli_out_writef(stdout, "selected_token_index: %llu\n", selected_index);
    }
    if (has_selected) {
        yvex_cli_out_writef(stdout, "selected_token_id: %u\n", selected_token);
    } else if (input) {
        yvex_cli_out_writef(stdout, "selected_token_id: unavailable\n");
    }
    yvex_cli_out_writef(stdout, "token_bounds_status: %s\n",
                        bounds_status ? bounds_status : "not-checked");
}

/* Parse, validate, and render an explicit token-list request. */
/* Purpose: Orchestrate the typed command input tokens request (`command_input_tokens`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_input_tokens(yvex_model_ref *ref,
                                const char *model_arg,
                                const char *tokens_text)
{
    yvex_token_input input;
    yvex_error err;
    unsigned long long vocab_size = 0ull;
    int rc;

    memset(&input, 0, sizeof(input));
    yvex_error_clear(&err);
    rc = yvex_token_input_parse_explicit(tokens_text, &input, &err);
    if (rc == YVEX_OK) rc = yvex_model_context_vocab_size(ref->path, &vocab_size, &err);
    if (rc == YVEX_OK) rc = yvex_token_input_validate_bounds(&input, vocab_size, &err);
    yvex_cli_out_writef(stdout, "token_input: tokens\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
                        ref->kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    yvex_cli_out_writef(stdout, "identity_status: %s\n",
                        ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered");
    print_token_input_summary(&input, rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                              0ull, input.token_count ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    yvex_cli_out_writef(stdout, "vocab_size: %llu\n", vocab_size);
    print_token_input_tokens(&input);
    yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(stdout, "status: %s\n",
                        rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
    yvex_model_ref_clear(ref);
    return rc == YVEX_OK ? 0 : print_yvex_error(&err, exit_for_status(rc));
}

/* Render an admitted-model identity failure without attempting tokenization. */
/* Purpose: Orchestrate the typed command input identity failure request (`command_input_identity_failure`). */
static int command_input_identity_failure(yvex_model_ref *ref,
                                          const char *subcommand,
                                          const char *model_arg,
                                          int rc)
{
    yvex_cli_out_writef(stdout, "token_input: %s\n", subcommand);
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "identity_status: fail\n");
    print_token_input_summary(NULL, "fail", "not-checked", 0ull, 0u, 0);
    yvex_cli_out_lines(stdout, literal_pair_2, sizeof(literal_pair_2) / sizeof(literal_pair_2[0]));
    yvex_model_ref_clear(ref);
    return exit_for_status(rc);
}

/* Render a prompt-input failure while preserving the historical output contract. */
/* Purpose: Orchestrate the typed command input prompt failure request (`command_input_prompt_failure`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_input_prompt_failure(yvex_model_ref *ref,
                                        yvex_model_context *ctx,
                                        const char *model_arg,
                                        const char *tokenizer_status,
                                        const char *reason,
                                        yvex_error *err,
                                        int rc)
{
    yvex_cli_out_writef(stdout, "token_input: prompt\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
                        ref->kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    yvex_cli_out_lines(stdout, literal_pair_1, sizeof(literal_pair_1) / sizeof(literal_pair_1[0]));
    yvex_cli_out_writef(stdout, "tokenizer_status: %s\n", tokenizer_status);
    if (ctx && ctx->tokenizer && strcmp(tokenizer_status, "unsupported") == 0) {
        yvex_cli_out_writef(stdout, "tokenizer_support: %s\n",
                            yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx->tokenizer)));
    }
    yvex_cli_out_writef(stdout, "reason: %s\n", reason);
    yvex_cli_out_lines(stdout, literal_lines_1, sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
    if (ctx && ctx->model) yvex_model_context_close(ctx);
    yvex_model_ref_clear(ref);
    return print_yvex_error(err, exit_for_status(rc));
}

/* Resolve and validate one explicit input request without executing generation. */
/* Purpose: Orchestrate the typed command input request (`command_input`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_input(int arg_count, char **args)
{
    yvex_model_ref ref;
    yvex_model_context ctx;
    yvex_token_input input;
    yvex_tokens tokens;
    yvex_error err;
    const char *subcommand;
    const char *model_arg = NULL;
    const char *tokens_text = NULL;
    const char *prompt_text = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));
    memset(&tokens, 0, sizeof(tokens));
    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_input_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }
    subcommand = args[2];
    if (strcmp(subcommand, "tokens") != 0 && strcmp(subcommand, "prompt") != 0) {
        yvex_cli_out_lines(stderr, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
        return 2;
    }
    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--tokens") == 0 && strcmp(subcommand, "tokens") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = args[++i];
        } else if (strcmp(args[i], "--text") == 0 && strcmp(subcommand, "prompt") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --text requires TEXT\n");
                return 2;
            }
            prompt_text = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown input option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help input' for usage.\n");
            return 2;
        }
    }
    if (!model_arg) {
        yvex_cli_out_writef(stderr, "yvex: input requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (strcmp(subcommand, "tokens") == 0 && !tokens_text) {
        yvex_cli_out_writef(stderr, "yvex: input tokens requires --tokens IDS\n");
        return 2;
    }
    if (strcmp(subcommand, "prompt") == 0 && !prompt_text) {
        yvex_cli_out_writef(stderr, "yvex: input prompt requires --text TEXT\n");
        return 2;
    }
    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    rc = enforce_registered_identity_cli(&ref, "input");
    if (rc != YVEX_OK) return command_input_identity_failure(&ref, subcommand, model_arg, rc);
    if (strcmp(subcommand, "tokens") == 0) return command_input_tokens(&ref, model_arg, tokens_text);

    rc = yvex_model_context_open(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        return command_input_prompt_failure(&ref, NULL, model_arg, "not-checked",
                                            yvex_error_message(&err), &err, rc);
    }
    rc = yvex_tokenizer_from_gguf(&ctx.tokenizer, ctx.gguf, ctx.model, &err);
    if (rc != YVEX_OK ||
        yvex_tokenizer_support_of(ctx.tokenizer) != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        const char *status = rc == YVEX_OK ? "unsupported" : "missing";
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return command_input_prompt_failure(&ref, &ctx, model_arg, status,
                                            "tokenizer-metadata-missing", &err,
                                            YVEX_ERR_UNSUPPORTED);
    }
    rc = yvex_tokenize_text(ctx.tokenizer, prompt_text, &tokens, &err);
    if (rc == YVEX_OK) {
        rc = yvex_token_input_from_ids(YVEX_TOKEN_INPUT_PROMPT_TEXT,
                                       tokens.ids, tokens.len, &input, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&input,
                                              yvex_tokenizer_vocab_size(ctx.tokenizer), &err);
    }
    yvex_cli_out_writef(stdout, "token_input: prompt\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
    yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
                        ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    yvex_cli_out_writef(stdout, "tokenizer_status: present\n");
    yvex_cli_out_writef(stdout, "tokenizer_support: %s\n",
                        yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    print_token_input_summary(&input, rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                              0ull, input.token_count ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    yvex_cli_out_writef(stdout, "vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    print_token_input_tokens(&input);
    yvex_cli_out_lines(stdout, literal_lines_2, sizeof(literal_lines_2) / sizeof(literal_lines_2[0]));
    yvex_cli_out_writef(stdout, "status: %s\n",
                        rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
    yvex_tokens_free(&tokens);
    yvex_model_context_close(&ctx);
    yvex_model_ref_clear(&ref);
    return rc == YVEX_OK ? 0 : print_yvex_error(&err, exit_for_status(rc));
}

/* Purpose: Orchestrate the typed detokenize command request (`yvex_detokenize_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_detokenize_command(int arg_count, char **args)
{
    return command_detokenize(arg_count, args);
}

/* Purpose: Orchestrate the typed input command request (`yvex_input_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_input_command(int arg_count, char **args)
{
    return command_input(arg_count, args);
}

/* Purpose: Orchestrate the typed prompt command request (`yvex_prompt_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_prompt_command(int arg_count, char **args)
{
    return command_prompt(arg_count, args);
}

/* Purpose: Orchestrate the typed tokenize command request (`yvex_tokenize_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_tokenize_command(int arg_count, char **args)
{
    return command_tokenize(arg_count, args);
}

/* Purpose: Orchestrate the typed tokenizer command request (`yvex_tokenizer_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_tokenizer_command(int arg_count, char **args)
{
    return command_tokenizer(arg_count, args);
}

/* Purpose: Render detokenize help from typed facts (`yvex_detokenize_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_detokenize_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex detokenize <path> --ids IDS\n\nDecodes comma-separated token IDs with the implemented "
            "tokenizer path.\n");
}

/* Purpose: Render input help from typed facts (`yvex_input_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_input_help(FILE *fp)
{
    yvex_cli_out_lines(fp, literal_lines_3, sizeof(literal_lines_3) / sizeof(literal_lines_3[0]));
}

/* Purpose: Render prompt help from typed facts (`yvex_prompt_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_prompt_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex prompt <path> [--system TEXT] --user TEXT [--assistant TEXT] [--tokens]\n\nPrompt "
            "renders the YVEX default prompt format. Arbitrary Jinja chat templates are not executed.\n");
}

/* Purpose: Render tokenize help from typed facts (`yvex_tokenize_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_tokenize_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex tokenize <path> --text TEXT\n\nEncodes text with the implemented tokenizer path.\n");
}

/* Purpose: Render tokenizer help from typed facts (`yvex_tokenizer_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_tokenizer_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex tokenizer <path>\n\nPrints tokenizer kind, support level, vocabulary facts, special "
            "token IDs, and chat template presence.\n");
}
