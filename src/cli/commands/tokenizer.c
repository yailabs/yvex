/*
 * tokenizer.c - tokenizer command surface.
 *
 * Owner: CLI tokenizer command.
 * Owns: tokenizer argv validation, dispatch, help, and compatibility rendering.
 * Does not own: tokenizer parsing, prompt semantics, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed tokenizer APIs and returns process exit status.
 */
#include "src/core/operator.h"
#include "src/cli/io/out.h"
#include <yvex/api.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Domain-owned command surface moved out of core.c. */

static int print_special_id_line(const char *name, int (*fn)(const yvex_tokenizer *, unsigned int *), const yvex_tokenizer *tokenizer)
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
        yvex_cli_out_writef(stderr, "usage: " "yvex tokenizer <path>\n");
        return 2;
    }

    rc = yvex_model_context_open_tokenizer(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "format: gguf\n");
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(yvex_model_arch(ctx.model)));
    yvex_cli_out_writef(stdout, "model_name: %s\n", yvex_model_name(ctx.model));
    yvex_cli_out_writef(stdout, "tokenizer_model: %s\n", yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(ctx.tokenizer)));
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
        } else if (strcmp(args[i], "--pieces") == 0 || strcmp(args[i], "--no-bos") == 0 || strcmp(args[i], "--eos") == 0) {
            /* Accepted for tokenizer layer CLI shape; fixture tokenization has no implicit BOS/EOS. */
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown tokenize option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help tokenize' for usage.\n");
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
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "status: detokenized\n");

    yvex_model_context_close(&ctx);
    return 0;
}

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

int yvex_detokenize_command(int arg_count, char **args)
{
    return command_detokenize(arg_count, args);
}

int yvex_prompt_command(int arg_count, char **args)
{
    return command_prompt(arg_count, args);
}

int yvex_tokenize_command(int arg_count, char **args)
{
    return command_tokenize(arg_count, args);
}

int yvex_tokenizer_command(int arg_count, char **args)
{
    return command_tokenizer(arg_count, args);
}

void yvex_detokenize_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex detokenize <path> --ids IDS\n\nDecodes comma-separated token IDs with the implemented tokenizer path.\n");
}

void yvex_prompt_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex prompt <path> [--system TEXT] --user TEXT [--assistant TEXT] [--tokens]\n\nPrompt renders the YVEX default prompt format. Arbitrary Jinja chat templates are not executed.\n");
}

void yvex_tokenize_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex tokenize <path> --text TEXT\n\nEncodes text with the implemented tokenizer path.\n");
}

void yvex_tokenizer_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex tokenizer <path>\n\nPrints tokenizer kind, support level, vocabulary facts, special token IDs, and chat template presence.\n");
}
