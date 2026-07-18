/*
 * runtime.c - runtime diagnostic command surface.
 *
 * Owner: CLI runtime command.
 * Owns: runtime argv validation, command dispatch, help, and compatibility rendering.
 * Does not own: engine/session lifecycle, graph semantics, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed runtime APIs and returns process exit status.
 */
#include "src/core/operator.h"
#include "src/cli/io/out.h"
#include <yvex/api.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void runtime_print_cuda_backend_unsupported(const char *status,
                                                   const yvex_error *err)
{
    yvex_cli_out_writef(stdout, "backend: cuda\n");
    yvex_cli_out_writef(stdout, "backend_status: unsupported\n");
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(err));
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "backend-unsupported");
}

static int backend_name_valid(const char *name)
{
    yvex_backend_kind kind;
    yvex_error err;

    yvex_error_clear(&err);
    return name && yvex_backend_kind_parse(name, &kind, &err) == YVEX_OK;
}

static int backend_name_is_cuda(const char *name)
{
    return name && strcmp(name, "cuda") == 0;
}

static void run_json_string(FILE *fp, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    yvex_cli_out_writef(fp, "\"");
    while (*p) {
        if (*p == '"' || *p == '\\') {
            yvex_cli_out_writef(fp, "\\%c", (int)*p);
        } else if (*p == '\n') {
            yvex_cli_out_writef(fp, "\\n");
        } else if (*p == '\r') {
            yvex_cli_out_writef(fp, "\\r");
        } else if (*p == '\t') {
            yvex_cli_out_writef(fp, "\\t");
        } else if (*p < 32u) {
            yvex_cli_out_writef(fp, "\\u%04x", (unsigned int)*p);
        } else {
            yvex_cli_out_writef(fp, "%c", (int)*p);
        }
        ++p;
    }
    yvex_cli_out_writef(fp, "\"");
}

static int chat_status_render(FILE *fp,
                              const yvex_chat_runtime *runtime,
                              yvex_error *err)
{
    yvex_session_summary summary;
    int rc;

    if (!fp || !runtime) return YVEX_ERR_INVALID_ARG;
    rc = yvex_chat_runtime_get_summary(runtime, &summary, err);
    if (rc != YVEX_OK) return rc;
    yvex_cli_out_writef(fp, "session_state: %s\n",
                        yvex_session_state_name(summary.state));
    yvex_cli_out_writef(fp, "position: %llu\n", summary.position);
    yvex_cli_out_writef(fp, "accepted_tokens: %llu\n",
                        summary.accepted_tokens);
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    yvex_cli_out_writef(fp, "generation: unsupported\n");
    return YVEX_OK;
}

static int run_render_plain(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) return YVEX_ERR_INVALID_ARG;
    yvex_cli_out_writef(fp, "run status: accepted-only\n");
    yvex_cli_out_writef(fp, "model: %s\n", result->model_name);
    yvex_cli_out_writef(fp, "backend: %s\n", result->backend_name);
    yvex_cli_out_writef(fp, "session_state: %s\n", result->session_state);
    yvex_cli_out_writef(fp, "prompt_tokens: %llu\n", result->prompt_tokens);
    yvex_cli_out_writef(fp, "accepted_tokens: %llu\n", result->accepted_tokens);
    yvex_cli_out_writef(fp, "position: %llu\n", result->position);
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    yvex_cli_out_writef(fp, "generation: %s\n", result->generation);
    yvex_cli_out_writef(fp, "reason: %s\n", result->reason);
    if (result->run_id[0]) yvex_cli_out_writef(fp, "run_id: %s\n", result->run_id);
    if (result->run_dir[0]) yvex_cli_out_writef(fp, "run_dir: %s\n", result->run_dir);
    if (result->metrics_out[0]) yvex_cli_out_writef(fp, "metrics_out: %s\n", result->metrics_out);
    if (result->trace_out[0]) yvex_cli_out_writef(fp, "trace_out: %s\n", result->trace_out);
    if (result->profile_out[0]) yvex_cli_out_writef(fp, "profile_out: %s\n", result->profile_out);
    return YVEX_OK;
}

static int run_render_json(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) return YVEX_ERR_INVALID_ARG;
    yvex_cli_out_writef(fp, "{\n  \"schema\": \"yvex.cli.result.v1\",\n");
    yvex_cli_out_writef(fp, "  \"command\": \"run\",\n  \"status\": \"accepted-only\",\n  \"data\": {\n");
    yvex_cli_out_writef(fp, "    \"model\": "); run_json_string(fp, result->model_name);
    yvex_cli_out_writef(fp, ",\n    \"backend\": "); run_json_string(fp, result->backend_name);
    yvex_cli_out_writef(fp, ",\n    \"session_state\": "); run_json_string(fp, result->session_state);
    yvex_cli_out_writef(fp, ",\n    \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    yvex_cli_out_writef(fp, "    \"accepted_tokens\": %llu,\n", result->accepted_tokens);
    yvex_cli_out_writef(fp, "    \"position\": %llu,\n    \"execution_ready\": false,\n    \"generation\": ", result->position);
    run_json_string(fp, result->generation);
    yvex_cli_out_writef(fp, ",\n    \"reason\": "); run_json_string(fp, result->reason);
    yvex_cli_out_writef(fp, ",\n    \"metrics\": {\n      \"prompt_tokens\": %llu,\n      \"accepted_tokens\": %llu\n    }",
                        result->prompt_tokens, result->accepted_tokens);
    if (result->metrics_out[0] || result->trace_out[0] ||
        result->profile_out[0] || result->run_dir[0]) {
        yvex_cli_out_writef(fp, ",\n    \"artifacts\": {\n      \"run_id\": ");
        run_json_string(fp, result->run_id);
        yvex_cli_out_writef(fp, ",\n      \"run_dir\": "); run_json_string(fp, result->run_dir);
        yvex_cli_out_writef(fp, ",\n      \"metrics_out\": "); run_json_string(fp, result->metrics_out);
        yvex_cli_out_writef(fp, ",\n      \"trace_out\": "); run_json_string(fp, result->trace_out);
        yvex_cli_out_writef(fp, ",\n      \"profile_out\": "); run_json_string(fp, result->profile_out);
        yvex_cli_out_writef(fp, "\n    }");
    }
    yvex_cli_out_writef(fp, "\n  },\n  \"error\": null\n}\n");
    return YVEX_OK;
}

static int status_line_render(FILE *fp,
                              const char *phase,
                              unsigned long long tokens,
                              unsigned long long position)
{
    if (!fp || !phase) return YVEX_ERR_INVALID_ARG;
    return yvex_cli_out_writef(fp, "[%s] tokens=%llu position=%llu\n",
                               phase, tokens, position) < 0
               ? YVEX_ERR_IO : YVEX_OK;
}


/* Shared runtime command helpers. */

int print_yvex_error(const yvex_error *err, int exit_code)
{
    yvex_cli_out_writef(stderr, "yvex: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

int exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

void print_quoted_bytes(const char *data, unsigned long long len)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "\"");
    for (i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        if (ch == '"' || ch == '\\') {
            yvex_cli_out_writef(stdout, "\\%c", (int)ch);
        } else if (ch == '\n') {
            yvex_cli_out_writef(stdout, "\\n");
        } else if (ch == '\r') {
            yvex_cli_out_writef(stdout, "\\r");
        } else if (ch == '\t') {
            yvex_cli_out_writef(stdout, "\\t");
        } else if (ch < 32 || ch > 126) {
            yvex_cli_out_writef(stdout, "\\x%02x", (unsigned int)ch);
        } else {
            yvex_cli_out_writef(stdout, "%c", (int)ch);
        }
    }
    yvex_cli_out_writef(stdout, "\"");
}

int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;
    yvex_model_ref ref;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));

    rc = yvex_model_ref_resolve(&ref, path, NULL, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    options.path = ref.path;
    options.readonly = 1;

    rc = yvex_artifact_open(artifact, &options, err);
    yvex_model_ref_clear(&ref);
    return rc;
}

void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int d;

    yvex_cli_out_writef(stdout, "[");
    for (d = 0; d < rank; ++d) {
        if (d > 0) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%llu", dims[d]);
    }
    yvex_cli_out_writef(stdout, "]");
}

void print_native_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    yvex_cli_out_writef(stdout, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%llu", dims[i]);
    }
    yvex_cli_out_writef(stdout, "]");
}

void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "ids:");
    for (i = 0; i < tokens->len; ++i) {
        yvex_cli_out_writef(stdout, " %u", tokens->ids[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
}

int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
{
    unsigned int *ids = NULL;
    unsigned long long len = 0;
    unsigned long long cap = 0;
    const char *p = text;

    *out_ids = NULL;
    *out_len = 0;

    while (*p) {
        char *end = NULL;
        unsigned long value;
        unsigned int *next;

        value = strtoul(p, &end, 10);
        if (end == p || value > 0xfffffffful) {
            free(ids);
            return 0;
        }
        if (len == cap) {
            unsigned long long next_cap = cap == 0 ? 8 : cap * 2u;
            if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*ids))) {
                free(ids);
                return 0;
            }
            next = (unsigned int *)realloc(ids, (size_t)next_cap * sizeof(*ids));
            if (!next) {
                free(ids);
                return 0;
            }
            ids = next;
            cap = next_cap;
        }
        ids[len++] = (unsigned int)value;
        if (*end == ',') {
            p = end + 1;
        } else if (*end == '\0') {
            p = end;
        } else {
            free(ids);
            return 0;
        }
    }

    *out_ids = ids;
    *out_len = len;
    return len > 0;
}

int parse_positive_ull(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

int parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

int parse_uint_allow_zero(const char *text, unsigned int *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 0xffffffffull) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

int parse_dims_csv(const char *text,
                          unsigned int rank,
                          unsigned long long dims[4])
{
    const char *p = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u) return 0;
    for (i = 0; i < 4u; ++i) dims[i] = 0;
    for (i = 0; i < rank; ++i) {
        unsigned long long value;
        errno = 0;
        value = strtoull(p, &end, 10);
        if (errno != 0 || end == p || value == 0) return 0;
        dims[i] = value;
        if (i + 1u < rank) {
            if (*end != ',') return 0;
            p = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
    return 1;
}

/* Token input and runtime command surfaces. */

static void print_token_input_tokens(const yvex_token_input *input)
{
    unsigned long long i;

    for (i = 0; input && i < input->token_count; ++i) {
        yvex_cli_out_writef(stdout, "token_%llu: %u\n", i, input->tokens[i]);
    }
}

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
    yvex_cli_out_writef(stdout, "token_bounds_status: %s\n", bounds_status ? bounds_status : "not-checked");
}

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
    unsigned long long vocab_size = 0ull;
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
        yvex_cli_out_writef(stderr, "yvex: input requires tokens or prompt\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
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
            yvex_cli_out_writef(stderr, "Try '" "yvex help input' for usage.\n");
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
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&ref, "input");
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(stdout, "token_input: %s\n", subcommand);
        yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
        yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
        yvex_cli_out_writef(stdout, "identity_status: fail\n");
        print_token_input_summary(NULL, "fail", "not-checked", 0ull, 0u, 0);
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    if (strcmp(subcommand, "tokens") == 0) {
        rc = yvex_token_input_parse_explicit(tokens_text, &input, &err);
        if (rc == YVEX_OK) {
            rc = yvex_model_context_vocab_size(ref.path, &vocab_size, &err);
        }
        if (rc == YVEX_OK) {
            rc = yvex_token_input_validate_bounds(&input, vocab_size, &err);
        }

        yvex_cli_out_writef(stdout, "token_input: tokens\n");
        yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
        yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
        yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        yvex_cli_out_writef(stdout, "identity_status: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered");
        print_token_input_summary(&input,
                                  rc == YVEX_OK ? "pass" : "fail",
                                  rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                                  0ull,
                                  input.token_count > 0ull ? input.tokens[0] : 0u,
                                  input.token_count > 0ull);
        yvex_cli_out_writef(stdout, "vocab_size: %llu\n", vocab_size);
        print_token_input_tokens(&input);
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
        yvex_model_ref_clear(&ref);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        return 0;
    }

    rc = yvex_model_context_open(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(stdout, "token_input: prompt\n");
        yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
        yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
        yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        yvex_cli_out_writef(stdout, "token_input_status: fail\n");
        yvex_cli_out_writef(stdout, "token_input_kind: prompt-text\n");
        yvex_cli_out_writef(stdout, "tokenizer_status: not-checked\n");
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "token_bounds_status: not-checked\n");
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported\n");
        yvex_cli_out_writef(stdout, "status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenizer_from_gguf(&ctx.tokenizer, ctx.gguf, ctx.model, &err);
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(stdout, "token_input: prompt\n");
        yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
        yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
        yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        yvex_cli_out_writef(stdout, "token_input_status: fail\n");
        yvex_cli_out_writef(stdout, "token_input_kind: prompt-text\n");
        yvex_cli_out_writef(stdout, "tokenizer_status: missing\n");
        yvex_cli_out_writef(stdout, "reason: tokenizer-metadata-missing\n");
        yvex_cli_out_writef(stdout, "token_bounds_status: not-checked\n");
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported\n");
        yvex_cli_out_writef(stdout, "status: token-input-fail\n");
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }
    if (yvex_tokenizer_support_of(ctx.tokenizer) != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        yvex_cli_out_writef(stdout, "token_input: prompt\n");
        yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
        yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
        yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        yvex_cli_out_writef(stdout, "token_input_status: fail\n");
        yvex_cli_out_writef(stdout, "token_input_kind: prompt-text\n");
        yvex_cli_out_writef(stdout, "tokenizer_status: unsupported\n");
        yvex_cli_out_writef(stdout, "tokenizer_support: %s\n",
               yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
        yvex_cli_out_writef(stdout, "reason: tokenizer-metadata-missing\n");
        yvex_cli_out_writef(stdout, "token_bounds_status: not-checked\n");
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported\n");
        yvex_cli_out_writef(stdout, "status: token-input-fail\n");
        yvex_model_context_close(&ctx);
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

    yvex_cli_out_writef(stdout, "token_input: prompt\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
    yvex_cli_out_writef(stdout, "model_input_kind: %s\n",
           ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    yvex_cli_out_writef(stdout, "tokenizer_status: present\n");
    yvex_cli_out_writef(stdout, "tokenizer_support: %s\n",
           yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    print_token_input_summary(&input,
                              rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                              input.token_bounds_checked ? "fail" : "not-checked",
                              0ull,
                              input.token_count > 0ull ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    yvex_cli_out_writef(stdout, "vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    print_token_input_tokens(&input);
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");

    yvex_tokens_free(&tokens);
    yvex_model_context_close(&ctx);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_engine(int arg_count, char **args)
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

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_engine_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires a file or alias\n");
                return 2;
            }
            if (model_arg) {
                yvex_cli_out_writef(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (args[i][0] == '-') {
            yvex_cli_out_writef(stderr, "yvex: unknown engine option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help engine' for usage.\n");
            return 2;
        } else {
            if (model_arg) {
                yvex_cli_out_writef(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = args[i];
        }
    }

    if (!model_arg) {
        yvex_cli_out_writef(stderr, "yvex: engine requires FILE_OR_ALIAS\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n");
        return 2;
    }
    if (backend_name && !backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
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
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        runtime_print_cuda_backend_unsupported("engine-backend-unsupported", &err);
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

    yvex_cli_out_writef(stdout, "engine status: %s\n", yvex_engine_status_name(summary.status));
    yvex_cli_out_writef(stdout, "format: gguf\n");
    yvex_cli_out_writef(stdout, "architecture: %s\n", summary.architecture);
    yvex_cli_out_writef(stdout, "model_name: %s\n", summary.model_name);
    yvex_cli_out_writef(stdout, "metadata_count: %llu\n", summary.metadata_count);
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", summary.tensor_count);
    yvex_cli_out_writef(stdout, "known_tensor_bytes: %llu\n", summary.known_tensor_bytes);
    yvex_cli_out_writef(stdout, "unsupported_tensor_accounting: %llu\n", summary.unsupported_tensor_accounting);
    yvex_cli_out_writef(stdout, "tokenizer_model: %s\n", summary.tokenizer_model);
    yvex_cli_out_writef(stdout, "tokenizer_support: %s\n", summary.tokenizer_support);
    yvex_cli_out_writef(stdout, "graph_status: %s\n", summary.graph_status);
    yvex_cli_out_writef(stdout, "weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    yvex_cli_out_writef(stdout, "weights_backend: %s\n", summary.weights_backend);
    yvex_cli_out_writef(stdout, "weight_tensor_count: %llu\n", summary.weight_tensor_count);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", summary.weight_total_bytes);
    yvex_cli_out_writef(stdout, "weight_backend_allocated_bytes: %llu\n", summary.weight_backend_allocated_bytes);
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
            yvex_cli_out_writef(stdout, "attached_weight_%llu: %s role=%s rank=%u dims=[",
                   j,
                   tensor->name ? tensor->name : "",
                   yvex_tensor_role_name(tensor->role),
                   tensor->rank);
            for (d = 0; d < tensor->rank; ++d) {
                if (d > 0) {
                    yvex_cli_out_writef(stdout, ",");
                }
                yvex_cli_out_writef(stdout, "%llu", tensor->dims[d]);
            }
            yvex_cli_out_writef(stdout, "] dtype=%s bytes=%llu\n",
                   yvex_dtype_name(tensor->dtype),
                   tensor->storage_bytes);
        }
    }
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_engine_diagnostic_reason(engine));
    yvex_cli_out_writef(stdout, "status: %s\n", summary.weights_attached ? "engine-weights-attached" : "engine-descriptor");

    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

typedef enum {
    YVEX_INFO_OUTPUT_NORMAL = 0,
    YVEX_INFO_OUTPUT_AUDIT
} yvex_info_output_mode;

static int parse_info_output_mode(const char *value, yvex_info_output_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_INFO_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_INFO_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static void print_info_normal(void)
{
    yvex_cli_out_writef(stdout, "info: YVEX %s\n", yvex_version_string());
    yvex_cli_out_writef(stdout, "runtime: bounded diagnostic generation\n");
    yvex_cli_out_writef(stdout, "cli_output: normal\n");
    yvex_cli_out_writef(stdout, "full_model_generation: unsupported\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "hint: use --" "audit for full diagnostic fields\n");
}

static void print_info_audit(void)
{
    yvex_cli_out_writef(stdout, "name: YVEX\n");
    yvex_cli_out_writef(stdout, "version: %s\n", yvex_version_string());
    yvex_cli_out_writef(stdout, "language: C\n");
    yvex_cli_out_writef(stdout, "interface: CLI-only\n");
    yvex_cli_out_writef(stdout, "status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token input boundary, prefill state foundation, minimal KV binding, minimal KV ownership, bounded decode/logits/sampling diagnostics, and bounded diagnostic generation loop with explicit append accounting\n");
    yvex_cli_out_writef(stdout, "library: libyvex.a\n");
    yvex_cli_out_writef(stdout, "filesystem: implemented\n");
    yvex_cli_out_writef(stdout, "artifact: open/read implemented\n");
    yvex_cli_out_writef(stdout, "gguf: metadata/tensor directory parsing implemented\n");
    yvex_cli_out_writef(stdout, "model: descriptor-only implemented\n");
    yvex_cli_out_writef(stdout, "tokenizer: fixture encode/decode implemented\n");
    yvex_cli_out_writef(stdout, "token_input: explicit token boundary implemented\n");
    yvex_cli_out_writef(stdout, "prefill_state: segment-summary foundation, bounded layer-backed prefill state, chunked prefill lifecycle, and minimal KV binding implemented\n");
    yvex_cli_out_writef(stdout, "prompt: default renderer implemented\n");
    yvex_cli_out_writef(stdout, "graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward primitive implemented\n");
    yvex_cli_out_writef(stdout, "planner: estimate-only implemented\n");
    yvex_cli_out_writef(stdout, "backend: CPU reference implemented\n");
    yvex_cli_out_writef(stdout, "backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is available\n");
    yvex_cli_out_writef(stdout, "weights: selected tensor materialization implemented\n");
    yvex_cli_out_writef(stdout, "engine: descriptor open and selected-weight attachment implemented\n");
    yvex_cli_out_writef(stdout, "session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented\n");
    yvex_cli_out_writef(stdout, "run: accepted-only runtime shell implemented\n");
    yvex_cli_out_writef(stdout, "chat: accepted-only REPL shell implemented\n");
    yvex_cli_out_writef(stdout, "metrics: runtime collector implemented\n");
    yvex_cli_out_writef(stdout, "trace: JSONL writer implemented\n");
    yvex_cli_out_writef(stdout, "profile: JSON writer implemented\n");
    yvex_cli_out_writef(stdout, "run_artifacts: metrics/trace/profile files implemented\n");
    yvex_cli_out_writef(stdout, "source_manifest: provenance JSON writer implemented\n");
    yvex_cli_out_writef(stdout, "native_weights: safetensors header inventory implemented\n");
    yvex_cli_out_writef(stdout, "gguf_template: contract validator implemented\n");
    yvex_cli_out_writef(stdout, "gguf_emit: controlled GGUF writer implemented\n");
    yvex_cli_out_writef(stdout, "conversion: open-weight selected tensor bridge implemented\n");
    yvex_cli_out_writef(stdout, "model_ref: alias-or-path resolver implemented\n");
    yvex_cli_out_writef(stdout, "model_registry: local model alias registry implemented\n");
    yvex_cli_out_writef(stdout, "quant_job: external quantization job manifest implemented\n");
    yvex_cli_out_writef(stdout, "qtype_support: conversion support matrix implemented\n");
    yvex_cli_out_writef(stdout, "weight_mapping: tensor adapter contract implemented\n");
    yvex_cli_out_writef(stdout, "quant_policy: manifest validator implemented\n");
    yvex_cli_out_writef(stdout, "imatrix: calibration artifact manifest implemented\n");
    yvex_cli_out_writef(stdout, "server_binary: yvexd shell implemented\n");
    yvex_cli_out_writef(stdout, "server_endpoints: health/metrics/models status implemented\n");
    yvex_cli_out_writef(stdout, "server_generation: not implemented\n");
    yvex_cli_out_writef(stdout, "kv: minimal session-owned append/read boundary implemented\n");
    yvex_cli_out_writef(stdout, "decode: bounded diagnostic state step implemented\n");
    yvex_cli_out_writef(stdout, "logits: bounded diagnostic buffer implemented\n");
    yvex_cli_out_writef(stdout, "sampling: bounded greedy sampler implemented\n");
    yvex_cli_out_writef(stdout, "generation_loop: bounded diagnostic loop implemented\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "inference: not implemented\n");
    yvex_cli_out_writef(stdout, "cuda: available when local driver/device probe succeeds\n");
    yvex_cli_out_writef(stdout, "server: yvexd status shell implemented\n");
}

static int command_info(int arg_count, char **args)
{
    yvex_info_output_mode output_mode = YVEX_INFO_OUTPUT_NORMAL;
    int i;

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_runtime_info_help(stdout);
            return 0;
        }
        if (strcmp(args[i], "--" "audit") == 0) {
            output_mode = YVEX_INFO_OUTPUT_AUDIT;
            continue;
        }
        if (strcmp(args[i], "--" "output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex info: --" "output requires normal|audit\n");
                return 2;
            }
            if (!parse_info_output_mode(args[++i], &output_mode)) {
                yvex_cli_out_writef(stderr, "yvex info: unsupported output mode: %s\n", args[i]);
                return 2;
            }
            continue;
        }
        yvex_cli_out_writef(stderr, "yvex info: unknown option: %s\n", args[i]);
        return 2;
    }

    if (output_mode == YVEX_INFO_OUTPUT_AUDIT) {
        print_info_audit();
    } else {
        print_info_normal();
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

    yvex_cli_out_writef(stdout, "prefill: state\n");
    yvex_cli_out_writef(stdout, "prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "segment-summary");
    yvex_cli_out_writef(stdout, "sequence_execution_mode: %s\n",
           summary && summary->sequence_execution_mode
               ? summary->sequence_execution_mode
               : "independent-token-segments");
    yvex_cli_out_writef(stdout, "prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "preflight");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n",
           summary && summary->backend_name && strcmp(summary->backend_name, "none") != 0
               ? summary->backend_name
               : (backend_name ? backend_name : "cpu"));
    yvex_cli_out_writef(stdout, "segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    yvex_cli_out_writef(stdout, "token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    yvex_cli_out_writef(stdout, "token_count: %llu\n", summary ? summary->token_count : 0ull);
    yvex_cli_out_writef(stdout, "tokens_processed: %llu\n", summary ? summary->tokens_processed : 0ull);
    yvex_cli_out_writef(stdout, "position_start: %llu\n", summary ? summary->position_start : 0ull);
    yvex_cli_out_writef(stdout, "position_end: %llu\n", summary ? summary->position_end : 0ull);
    yvex_cli_out_writef(stdout, "failed_token_index: %llu\n", summary ? summary->failed_token_index : 0ull);
    yvex_cli_out_writef(stdout, "context_boundary_status: %s\n",
           summary && summary->context_boundary_status ? summary->context_boundary_status : "unchecked");
    yvex_cli_out_writef(stdout, "context_length: %llu\n", summary ? summary->context_length : 0ull);
    yvex_cli_out_writef(stdout, "chunked_prefill_requested: %s\n",
           summary && summary->chunked_prefill_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "chunk_execution_mode: %s\n",
           summary && summary->chunk_execution_mode ? summary->chunk_execution_mode : "token-loop");
    yvex_cli_out_writef(stdout, "chunk_size: %llu\n", summary ? summary->chunk_size : 0ull);
    yvex_cli_out_writef(stdout, "chunk_count: %llu\n", summary ? summary->chunk_count : 0ull);
    yvex_cli_out_writef(stdout, "chunks_processed: %llu\n", summary ? summary->chunks_processed : 0ull);
    yvex_cli_out_writef(stdout, "failed_chunk_index: %llu\n", summary ? summary->failed_chunk_index : 0ull);
    yvex_cli_out_writef(stdout, "current_chunk_start: %llu\n", summary ? summary->current_chunk_start : 0ull);
    yvex_cli_out_writef(stdout, "current_chunk_end: %llu\n", summary ? summary->current_chunk_end : 0ull);
    yvex_cli_out_writef(stdout, "final_chunk_checksum: %llu\n", summary ? summary->final_chunk_checksum : 0ull);
    yvex_cli_out_writef(stdout, "prefill_scratch_kind: %s\n",
           summary && summary->prefill_scratch_kind ? summary->prefill_scratch_kind : "host-diagnostic-reuse");
    yvex_cli_out_writef(stdout, "prefill_scratch_reuse: %s\n",
           summary && summary->prefill_scratch_reuse ? "true" : "false");
    yvex_cli_out_writef(stdout, "prefill_scratch_allocations: %llu\n",
           summary ? summary->prefill_scratch_allocations : 0ull);
    yvex_cli_out_writef(stdout, "prefill_scratch_reuse_count: %llu\n",
           summary ? summary->prefill_scratch_reuse_count : 0ull);
    yvex_cli_out_writef(stdout, "prefill_scratch_cleanup_attempted: %s\n",
           summary && summary->prefill_scratch_cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "prefill_scratch_cleanup_status: %s\n",
           summary && summary->prefill_scratch_cleanup_status
               ? summary->prefill_scratch_cleanup_status
               : "not-needed");
    yvex_cli_out_writef(stdout, "segment_graph_executions: %llu\n", summary ? summary->segment_graph_executions : 0ull);
    yvex_cli_out_writef(stdout, "segment_output_count: %llu\n", summary ? summary->segment_output_count : 0ull);
    yvex_cli_out_writef(stdout, "segment_output_bytes: %llu\n", summary ? summary->segment_output_bytes : 0ull);
    yvex_cli_out_writef(stdout, "prefill_aggregate_checksum: %llu\n", summary ? summary->aggregate_checksum : 0ull);
    yvex_cli_out_writef(stdout, "prefill_final_token_checksum: %llu\n", summary ? summary->final_token_checksum : 0ull);
    yvex_cli_out_writef(stdout, "prefill_total_output_bytes: %llu\n", summary ? summary->total_output_bytes : 0ull);
    yvex_cli_out_writef(stdout, "prefill_scratch_bytes: %llu\n", summary ? summary->scratch_bytes : 0ull);
    yvex_cli_out_writef(stdout, "prefill_max_abs_diff: %.9g\n", summary ? summary->max_abs_diff : 0.0);
    yvex_cli_out_writef(stdout, "layer_prefill_requested: %s\n",
           summary && summary->layer_prefill_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "layer_execution_kind: %s\n",
           summary && summary->layer_execution_kind ? summary->layer_execution_kind : "none");
    yvex_cli_out_writef(stdout, "model_layer_execution: %s\n",
           summary && summary->model_layer_execution ? "true" : "false");
    yvex_cli_out_writef(stdout, "layer_input_projection: %s\n",
           summary && summary->layer_input_projection ? summary->layer_input_projection : "none");
    yvex_cli_out_writef(stdout, "layer_handoff: %s\n",
           summary && summary->layer_handoff ? summary->layer_handoff : "none");
    yvex_cli_out_writef(stdout, "layer_sequence_rebuild: %s\n",
           summary && summary->layer_sequence_rebuild ? summary->layer_sequence_rebuild : "none");
    yvex_cli_out_writef(stdout, "layer_count: %llu\n", summary ? summary->layer_count : 0ull);
    yvex_cli_out_writef(stdout, "layer_graph_executions: %llu\n", summary ? summary->layer_graph_executions : 0ull);
    yvex_cli_out_writef(stdout, "layer_block_executions: %llu\n", summary ? summary->layer_block_executions : 0ull);
    yvex_cli_out_writef(stdout, "layer_total_op_count: %llu\n", summary ? summary->layer_total_op_count : 0ull);
    yvex_cli_out_writef(stdout, "layer_output_count: %llu\n", summary ? summary->layer_output_count : 0ull);
    yvex_cli_out_writef(stdout, "layer_output_bytes: %llu\n", summary ? summary->layer_output_bytes : 0ull);
    yvex_cli_out_writef(stdout, "layer_total_output_bytes: %llu\n", summary ? summary->layer_total_output_bytes : 0ull);
    yvex_cli_out_writef(stdout, "layer_total_scratch_bytes: %llu\n", summary ? summary->layer_total_scratch_bytes : 0ull);
    yvex_cli_out_writef(stdout, "layer_final_checksum: %llu\n", summary ? summary->layer_final_checksum : 0ull);
    yvex_cli_out_writef(stdout, "layer_final_reference_checksum: %llu\n",
           summary ? summary->layer_final_reference_checksum : 0ull);
    yvex_cli_out_writef(stdout, "layer_max_abs_diff: %.9g\n", summary ? summary->layer_max_abs_diff : 0.0);
    yvex_cli_out_writef(stdout, "layer_output_sample_values:");
    if (summary && summary->layer_output_sample_count > 0ull) {
        for (i = 0; i < summary->layer_output_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",",
                   (double)summary->layer_output_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    if (summary && summary->cuda_parity) {
        yvex_cli_out_writef(stdout, "prefill_cuda_parity: pass\n");
    }
    yvex_cli_out_writef(stdout, "kv_ready: %s\n", summary && summary->kv_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "session_kv_owned: %s\n",
           summary && summary->session_kv_owned ? "true" : "false");
    yvex_cli_out_writef(stdout, "kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "kv_binding_kind: %s\n",
           summary && summary->kv_binding_kind ? summary->kv_binding_kind : "none");
    yvex_cli_out_writef(stdout, "kv_binding_source: %s\n",
           summary && summary->kv_binding_source ? summary->kv_binding_source : "segment-output-sample");
    yvex_cli_out_writef(stdout, "kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    yvex_cli_out_writef(stdout, "kv_owner: %s\n",
           summary && summary->kv_owner ? summary->kv_owner : "none");
    yvex_cli_out_writef(stdout, "kv_dtype: %s\n",
           summary && summary->kv_dtype ? summary->kv_dtype : "none");
    yvex_cli_out_writef(stdout, "kv_layers: %llu\n", summary ? summary->kv_layers : 0ull);
    yvex_cli_out_writef(stdout, "kv_heads: %llu\n", summary ? summary->kv_heads : 0ull);
    yvex_cli_out_writef(stdout, "kv_head_dim: %llu\n", summary ? summary->kv_head_dim : 0ull);
    yvex_cli_out_writef(stdout, "kv_capacity: %llu\n", summary ? summary->kv_capacity : 0ull);
    yvex_cli_out_writef(stdout, "kv_values_per_position: %llu\n", summary ? summary->kv_values_per_position : 0ull);
    yvex_cli_out_writef(stdout, "kv_bytes_per_position: %llu\n", summary ? summary->kv_bytes_per_position : 0ull);
    yvex_cli_out_writef(stdout, "kv_planned_bytes: %llu\n", summary ? summary->kv_planned_bytes : 0ull);
    yvex_cli_out_writef(stdout, "kv_allocated_bytes: %llu\n", summary ? summary->kv_allocated_bytes : 0ull);
    yvex_cli_out_writef(stdout, "kv_positions_written: %llu\n", summary ? summary->kv_positions_written : 0ull);
    yvex_cli_out_writef(stdout, "kv_append_count: %llu\n", summary ? summary->kv_append_count : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_count: %llu\n", summary ? summary->kv_read_count : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_position: %llu\n", summary ? summary->kv_read_position : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_value_count: %llu\n", summary ? summary->kv_read_value_count : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_sample_values:");
    if (summary && summary->kv_read_sample_count > 0ull) {
        for (i = 0; i < summary->kv_read_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)summary->kv_read_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "kv_overflow: %s\n",
           summary && summary->kv_overflow_status ? summary->kv_overflow_status : "not-checked");
    yvex_cli_out_writef(stdout, "kv_cleanup_status: %s\n",
           summary && summary->kv_cleanup_status ? summary->kv_cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "prefill-state-fail");
}

/* Segment-summary prefill command surface. */

static void init_prefill_summary_cli_defaults(yvex_prefill_state_summary *summary,
                                              const char *segment_name,
                                              int attach_kv,
                                              const yvex_kv_shape *shape,
                                              unsigned long long layer_count,
                                              unsigned long long chunk_size,
                                              unsigned long long position_start,
                                              unsigned long long context_length)
{
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->prefill_state_kind = layer_count > 0ull
                                      ? "layer-backed-segment-summary"
                                      : "segment-summary";
    summary->sequence_execution_mode = layer_count > 0ull
                                           ? "segment-then-controlled-layer-fixture"
                                           : "independent-token-segments";
    summary->prefill_phase = "preflight";
    summary->segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    summary->position_start = position_start;
    summary->position_end = position_start;
    summary->chunked_prefill_requested = chunk_size > 0ull;
    summary->chunk_execution_mode = chunk_size > 0ull ? "bounded-token-chunks" : "token-loop";
    summary->chunk_size = chunk_size;
    summary->context_length = context_length;
    summary->context_boundary_status = "unchecked";
    summary->prefill_scratch_kind = "host-diagnostic-reuse";
    summary->prefill_scratch_reuse = chunk_size > 0ull;
    summary->prefill_scratch_cleanup_status = "not-needed";
    summary->cleanup_status = "not-needed";
    summary->generation_status = "unsupported";
    summary->layer_prefill_requested = layer_count > 0ull;
    summary->layer_execution_kind = layer_count > 0ull ? "controlled-layer-fixture" : "none";
    summary->layer_input_projection = layer_count > 0ull ? "segment-sample-prefix" : "none";
    summary->layer_handoff = layer_count > 0ull ? "selected-position-row" : "none";
    summary->layer_sequence_rebuild = layer_count > 0ull
                                          ? "deterministic-with-previous-position-row"
                                          : "none";
    summary->layer_count = layer_count;
    summary->kv_binding_kind = attach_kv ? "minimal-diagnostic" : "none";
    summary->kv_binding_source = layer_count > 0ull
                                     ? "layer-final-sample"
                                     : "segment-output-sample";
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

static int command_prefill(int arg_count, char **args)
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
    unsigned long long layer_count = 0ull;
    unsigned long long layer_hidden_dim = 0ull;
    unsigned long long layer_head_dim = 0ull;
    unsigned long long layer_ffn_dim = 0ull;
    unsigned long long chunk_size = 0ull;
    unsigned long long position_start = 0ull;
    unsigned long long context_length = 0ull;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int layer_count_seen = 0;
    int layer_hidden_seen = 0;
    int layer_head_seen = 0;
    int layer_ffn_seen = 0;
    int chunk_size_seen = 0;
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

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_prefill_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--segment") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = args[++i];
        } else if (strcmp(args[i], "--tokens") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = args[++i];
        } else if (strcmp(args[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(args[i], "--kv-layers") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-heads") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.kv_head_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-head-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-capacity") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.capacity)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layers") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_hidden_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-head-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_ffn_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--chunk-size") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &chunk_size)) {
                yvex_cli_out_writef(stderr, "yvex: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--position-start") == 0) {
            if (i + 1 >= arg_count || !parse_ull_allow_zero(args[i + 1], &position_start)) {
                yvex_cli_out_writef(stderr, "yvex: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--context-length") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --context-length requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (!model_arg) {
            model_arg = args[i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown prefill option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help prefill' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !tokens_text || !segment_name) {
        yvex_cli_out_writef(stderr, "usage: " "yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (!backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unsupported prefill segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        yvex_cli_out_writef(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        yvex_cli_out_writef(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (layer_count_seen && (layer_count == 0ull || layer_count > 16ull)) {
        yvex_cli_out_writef(stderr, "yvex: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (layer_count_seen && !layer_hidden_seen) {
        layer_hidden_dim = 8ull;
        layer_head_dim = 8ull;
        layer_ffn_dim = 16ull;
    }
    if (layer_count_seen && layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_cli_out_writef(stderr, "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
                (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "prefill");
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name,
                                          attach_kv, &kv_shape, layer_count,
                                          chunk_size, position_start,
                                          context_length);
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = yvex_model_context_vocab_size(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name,
                                          attach_kv, &kv_shape, layer_count,
                                          chunk_size, position_start,
                                          context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_preflight(&model_ref,
                               backend_name,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name,
                                          attach_kv, &kv_shape, layer_count,
                                          chunk_size, position_start,
                                          context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_cli_graph_guard_print(&graph_guard);
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
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name,
                                          attach_kv, &kv_shape, layer_count,
                                          chunk_size, position_start,
                                          context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    prefill_options.token_input = &token_input;
    prefill_options.segment_name = segment_name;
    prefill_options.position_start = position_start;
    prefill_options.chunk_size = chunk_size_seen ? chunk_size : 0ull;
    prefill_options.context_length = context_length;
    prefill_options.attach_kv = attach_kv;
    prefill_options.kv_shape = kv_shape;
    prefill_options.layer_count = layer_count_seen ? layer_count : 0ull;
    prefill_options.layer_hidden_dim = layer_hidden_dim;
    prefill_options.layer_head_dim = layer_head_dim;
    prefill_options.layer_ffn_dim = layer_ffn_dim;
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

static int command_plan(int arg_count, char **args)
{
    yvex_model_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.backend_name = "cpu";

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_plan_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--seq") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &options.sequence_length)) {
                yvex_cli_out_writef(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--ctx") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &options.context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            options.backend_name = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown plan option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help plan' for usage.\n");
            return 2;
        }
    }

    rc = yvex_model_context_open(args[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_dump(plan, stdout, &err);
    }

    yvex_plan_close(plan);
    yvex_model_context_close(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
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
                                         int arg_count,
                                         char **args,
                                         yvex_error *err)
{
    yvex_profile_summary profile;
    yvex_metric_counters counters;
    int rc;

    if (!artifacts || !metrics) {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_run_artifacts_write_command(artifacts, arg_count, args, err);
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

/* Accepted-only run diagnostics. */

static int command_run(int arg_count, char **args)
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

    if (arg_count == 2 || (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0))) {
        yvex_run_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--prompt") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --prompt requires a value\n");
                return 2;
            }
            prompt_text = args[++i];
        } else if (strcmp(args[i], "--system") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --system requires a value\n");
                return 2;
            }
            system_text = args[++i];
        } else if (strcmp(args[i], "--ctx") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--" "output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --" "output requires plain or json\n");
                return 2;
            }
            output = args[++i];
            if (strcmp(output, "plain") != 0 && strcmp(output, "json") != 0) {
                yvex_cli_out_writef(stderr, "yvex: --" "output must be plain or json\n");
                return 2;
            }
        } else if (strcmp(args[i], "--status-line") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
            status_line = args[++i];
            if (strcmp(status_line, "auto") != 0 && strcmp(status_line, "off") != 0 &&
                strcmp(status_line, "always") != 0) {
                yvex_cli_out_writef(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
        } else if (strcmp(args[i], "--metrics-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = args[++i];
        } else if (strcmp(args[i], "--trace-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = args[++i];
        } else if (strcmp(args[i], "--profile-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = args[++i];
        } else if (strcmp(args[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(args[i], "--run-dir") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown run option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help run' for usage.\n");
            return 2;
        }
    }

    if (!model_path) {
        yvex_cli_out_writef(stderr, "yvex: --model is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!backend_name) {
        yvex_cli_out_writef(stderr, "yvex: --backend is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!prompt_text) {
        yvex_cli_out_writef(stderr, "yvex: --prompt is required for yvex run in diagnostic runtime\n");
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
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        yvex_cli_out_writef(stdout, "run status: backend-unsupported\n");
        yvex_cli_out_writef(stdout, "backend: cuda\n");
        yvex_cli_out_writef(stdout, "execution_ready: false\n");
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
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
        (void)status_line_render(stderr, "accept", result.prompt_tokens, result.position);
    }

    if (strcmp(output, "json") == 0) {
        (void)run_render_json(stdout, &result);
    } else {
        (void)run_render_plain(stdout, &result);
    }

    yvex_trace_close(trace);
    rc = cli_write_observability_files("run", result.model_name, result.backend_name,
                                       "accepted-only", &artifacts, metrics, arg_count, args, &err);
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

static int command_session(int arg_count, char **args)
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

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_session_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--ctx") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &session_options.context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--text") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = args[++i];
        } else if (strcmp(args[i], "--accept-tokens") == 0) {
            accept_tokens = 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown session option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help session' for usage.\n");
            return 2;
        }
    }

    if (!backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, args[2], NULL, &err);
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
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        runtime_print_cuda_backend_unsupported("session-backend-unsupported", &err);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_kind_parse(backend_name, &backend_options.kind, &err);
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        yvex_engine_close(engine);
        runtime_print_cuda_backend_unsupported("session-backend-unsupported", &err);
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

    yvex_cli_out_writef(stdout, "engine_status: %s\n", summary.engine_status);
    yvex_cli_out_writef(stdout, "backend: %s\n", summary.backend_kind);
    yvex_cli_out_writef(stdout, "backend_status: %s\n", summary.backend_status);
    yvex_cli_out_writef(stdout, "session_state: %s\n", yvex_session_state_name(summary.state));
    yvex_cli_out_writef(stdout, "context_length: %llu\n", summary.context_length);
    yvex_cli_out_writef(stdout, "position: %llu\n", summary.position);
    yvex_cli_out_writef(stdout, "accepted_tokens: %llu\n", summary.accepted_tokens);
    yvex_cli_out_writef(stdout, "kv_status: %s\n", summary.kv_status);
    yvex_cli_out_writef(stdout, "kv_bytes: %llu\n", summary.kv_bytes);
    yvex_cli_out_writef(stdout, "logits_status: %s\n", summary.logits_status);
    yvex_cli_out_writef(stdout, "weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    yvex_cli_out_writef(stdout, "weights_backend: %s\n", summary.weights_backend);
    yvex_cli_out_writef(stdout, "weight_tensor_count: %llu\n", summary.weight_tensor_count);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", summary.weight_total_bytes);
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        yvex_cli_out_writef(stdout, "tokens: %llu\n", tokens.len);
    }
    yvex_cli_out_writef(stdout, "status: %s\n", accept_tokens ? "session-token-accepted" : "session-created");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static void print_chat_slash_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "commands:\n");
    yvex_cli_out_writef(fp, "  /help\n");
    yvex_cli_out_writef(fp, "  /status\n");
    yvex_cli_out_writef(fp, "  /model\n");
    yvex_cli_out_writef(fp, "  /backend\n");
    yvex_cli_out_writef(fp, "  /tokens\n");
    yvex_cli_out_writef(fp, "  /reset\n");
    yvex_cli_out_writef(fp, "  /quit\n");
}

static void print_chat_model(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_engine_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_engine_get_summary(runtime->engine, &summary, &err) == YVEX_OK) {
        yvex_cli_out_writef(fp, "model: %s\n", summary.model_name);
        yvex_cli_out_writef(fp, "architecture: %s\n", summary.architecture);
        yvex_cli_out_writef(fp, "tokenizer: %s\n", summary.tokenizer_model);
    }
}

static void print_chat_backend(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_cli_out_writef(fp, "backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(runtime->backend)));
    yvex_cli_out_writef(fp, "status: %s\n", yvex_backend_status_name(yvex_backend_status_of(runtime->backend)));
    yvex_cli_out_writef(fp, "capabilities:\n");
    yvex_cli_out_writef(fp, "  tensor_alloc: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ? "yes" : "no");
    yvex_cli_out_writef(fp, "  tensor_read_write: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE) ? "yes" : "no");
    yvex_cli_out_writef(fp, "  op_embed: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_EMBED) ? "yes" : "no");
    yvex_cli_out_writef(fp, "  op_mlp: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_MLP) ? "yes" : "no");
    yvex_cli_out_writef(fp, "  op_rope: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_ROPE) ? "yes" : "no");
}

static void print_chat_tokens(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_session_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_chat_runtime_get_summary(runtime, &summary, &err) == YVEX_OK) {
        yvex_cli_out_writef(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
        yvex_cli_out_writef(fp, "position: %llu\n", summary.position);
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
        return chat_status_render(stdout, runtime, err);
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
        yvex_cli_out_writef(stdout, "session reset\n");
        yvex_cli_out_writef(stdout, "position: 0\n");
        return YVEX_OK;
    case YVEX_SLASH_QUIT:
        yvex_cli_out_writef(stdout, "bye\n");
        *done = 1;
        return YVEX_OK;
    case YVEX_SLASH_UNKNOWN:
        yvex_cli_out_writef(stdout, "unknown slash command: %s\n", line ? line : "");
        yvex_cli_out_writef(stdout, "type /help\n");
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

/* Accepted-only diagnostic REPL command surface. */

static int command_chat(int arg_count, char **args)
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

    if (arg_count == 2 || (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0))) {
        yvex_chat_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--ctx") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--metrics-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = args[++i];
        } else if (strcmp(args[i], "--trace-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = args[++i];
        } else if (strcmp(args[i], "--profile-out") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = args[++i];
        } else if (strcmp(args[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(args[i], "--run-dir") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown chat option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help chat' for usage.\n");
            return 2;
        }
    }

    if (!backend_name) {
        yvex_cli_out_writef(stderr, "yvex: --backend is required for yvex chat in diagnostic runtime\n");
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
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        runtime_print_cuda_backend_unsupported("chat-backend-unsupported", &err);
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
    yvex_cli_out_writef(stdout, "YVEX chat runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", engine_summary.model_name);
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "session_state: %s\n", yvex_session_state_name(session_summary.state));
    yvex_cli_out_writef(stdout, "generation: unsupported in diagnostic runtime\n");
    yvex_cli_out_writef(stdout, "type /help for commands\n");

    while (!done) {
        yvex_cli_out_writef(stdout, "yvex> ");
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
            yvex_cli_out_writef(stdout, "accepted tokens: %llu\n", result.prompt_tokens);
            yvex_cli_out_writef(stdout, "position: %llu\n", result.position);
            yvex_cli_out_writef(stdout, "assistant: [generation unsupported in diagnostic runtime]\n");
        }
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "accepted-only",
                          "chat runtime exited without generation", &err);
    yvex_trace_close(trace);
    rc = cli_write_observability_files("chat", engine_summary.model_name, backend_name,
                                       "accepted-only", &artifacts, metrics, arg_count, args, &err);
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

int yvex_chat_command(int arg_count, char **args)
{
    return command_chat(arg_count, args);
}

/*
 * yvex_engine_command()
 *
 * Purpose:
 *   dispatch engine diagnostic CLI requests to the existing runtime command
 *   implementation.
 *
 * Inputs:
 *   arg_count/args are borrowed command-line arguments.
 *
 * Effects:
 *   may open descriptors and print diagnostic output through the runtime command
 *   path; it does not implement graph kernels or generation.
 *
 * Failure:
 *   returns parser or runtime-command failure codes from the delegated path.
 *
 * Boundary:
 *   engine diagnostics do not claim full runtime support, generation, eval,
 *   benchmark, throughput, or release readiness.
 */
int yvex_engine_command(int arg_count, char **args)
{
    return command_engine(arg_count, args);
}

int yvex_runtime_info_command(int arg_count, char **args)
{
    return command_info(arg_count, args);
}

int yvex_input_command(int arg_count, char **args)
{
    return command_input(arg_count, args);
}

int yvex_plan_command(int arg_count, char **args)
{
    return command_plan(arg_count, args);
}

int yvex_prefill_command(int arg_count, char **args)
{
    return command_prefill(arg_count, args);
}

int yvex_run_command(int arg_count, char **args)
{
    return command_run(arg_count, args);
}

int yvex_session_command(int arg_count, char **args)
{
    return command_session(arg_count, args);
}

void yvex_chat_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex chat [--model FILE_OR_ALIAS] --backend cpu|cuda [--ctx N] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]\n\nChat opens diagnostic engine/backend/session state and accepts user text without generating output. If --model is omitted, chat uses the current model selected with yvex models use ALIAS.\n");
}

void yvex_engine_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n\nEngine opens descriptor/tokenizer/graph state and can observe selected materialized residency. It does not execute prefill, decode, or generation.\n");
}

void yvex_runtime_info_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex info [--" "audit | --" "output normal|audit]\n\nPrints the implemented build and runtime boundary status. Normal output is compact; audit output preserves full diagnostic fields.\n");
}

void yvex_input_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex input tokens --model FILE_OR_ALIAS --tokens IDS\n");
    yvex_cli_out_writef(fp, "       yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
    yvex_cli_out_writef(fp, "\nInput parses explicit tokens or tokenizer-backed prompt text into validated token input.\n");
}

void yvex_plan_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx N]\n\nPlan builds graph and memory estimates. Execution remains disabled.\n");
}

void yvex_prefill_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n\nPrefill records a segment-summary prefill foundation from validated token input and can bind processed positions to minimal KV ownership. Layer-backed prefill uses the selected embedding+RMSNorm segment plus a controlled layer fixture scheduler over a sampled row. Chunked prefill partitions validated token input into bounded diagnostic chunks with explicit scratch and context-boundary reporting. It is not full transformer prefill, decode, logits, sampling, or generation.\n");
}

void yvex_run_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex run --model FILE --backend cpu|cuda --prompt TEXT [--system TEXT] [--" "output plain|json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]\n\nRun accepts one prompt through the diagnostic runtime path and reports accepted-only diagnostics.\n");
}

void yvex_session_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex session FILE_OR_ALIAS [--backend cpu|cuda] [--ctx N] [--text TEXT] [--accept-tokens]\n\nSession creates a lifecycle diagnostic session over engine/backend state. It does not run prefill, decode, or generation.\n");
}
