/* Owner: CLI runtime command.
 * Owns: runtime argv validation, command dispatch, help, and compatibility rendering.
 * Does not own: engine/session lifecycle, graph semantics, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed runtime APIs and returns process exit status.
 * Purpose: provide runtime argv validation, command dispatch, help, and compatibility rendering.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/io/private.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/generation.h>
#include <yvex/graph.h>
#include <yvex/internal/runtime.h>
#include <yvex/metrics.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <yvex/runtime.h>
#include <yvex/tokenizer.h>

static const char *const literal_pair_0[] = {"generation: unsupported in diagnostic runtime",
                                             "type /help for commands"};

static const char *const literal_pair_1[] = {"session reset", "position: 0"};

static const char *const literal_pair_2[] = {"execution_ready: false",
                                             "graph_execution_ready: false"};

static const char *const literal_pair_3[] = {"execution_ready: false",
                                             "graph_execution_ready: false"};

static const char *const literal_pair_4[] = {
    "{\n  \"schema\": \"yvex.cli.result.v1\",",
    "  \"command\": \"run\",\n  \"status\": \"accepted-only\",\n  \"data\": {"};

static const char *const literal_pair_5[] = {"execution_ready: false", "generation: unsupported"};

static const char *const literal_pair_6[] = {"backend: cuda", "backend_status: unsupported"};

static const char *const literal_lines_0[] = {
    "runtime: bounded diagnostic generation", "cli_output: normal",
    "full_model_generation: unsupported", "benchmark_status: not-measured"};

static const char *const literal_lines_1[] = {
    "language: C",
    "interface: CLI-only",
    "status: selected tensor materialization, engine weight attachment, fixture graph execution, "
    "real "
    "selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token "
    "input "
    "boundary, prefill state foundation, minimal KV binding, minimal KV ownership, bounded "
    "decode/logits/"
    "sampling diagnostics, and bounded diagnostic generation loop with explicit append accounting",
    "library: libyvex.a",
    "filesystem: implemented",
    "artifact: open/read implemented",
    "gguf: metadata/tensor directory parsing implemented",
    "model: descriptor-only implemented",
    "tokenizer: fixture encode/decode implemented",
    "token_input: explicit token boundary implemented",
    "prefill_state: segment-summary foundation, bounded layer-backed prefill state, chunked "
    "prefill "
    "lifecycle, and minimal KV binding implemented",
    "prompt: default renderer implemented",
    "graph: partial planning, deterministic fixture execution, selected embedding partial "
    "execution, "
    "selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 "
    "attention "
    "primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward "
    "primitive "
    "implemented",
    "planner: estimate-only implemented",
    "backend: CPU reference implemented",
    "backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention "
    "primitive, "
    "F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is "
    "available",
    "weights: selected tensor materialization implemented",
    "engine: descriptor open and selected-weight attachment implemented",
    "session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented",
    "run: accepted-only runtime shell implemented",
    "chat: accepted-only REPL shell implemented",
    "metrics: runtime collector implemented",
    "trace: JSONL writer implemented",
    "profile: JSON writer implemented",
    "run_artifacts: metrics/trace/profile files implemented",
    "source_manifest: provenance JSON writer implemented",
    "native_weights: safetensors header inventory implemented",
    "gguf_template: contract validator implemented",
    "gguf_emit: controlled GGUF writer implemented",
    "conversion: open-weight selected tensor bridge implemented",
    "model_ref: alias-or-path resolver implemented",
    "model_registry: local model alias registry implemented",
    "quant_job: external quantization job manifest implemented",
    "qtype_support: conversion support matrix implemented",
    "weight_mapping: tensor adapter contract implemented",
    "quant_policy: manifest validator implemented",
    "imatrix: calibration artifact manifest implemented",
    "server_binary: yvexd shell implemented",
    "server_endpoints: health/metrics/models status implemented",
    "server_generation: not implemented",
    "kv: minimal session-owned append/read boundary implemented",
    "decode: bounded diagnostic state step implemented",
    "logits: bounded diagnostic buffer implemented",
    "sampling: bounded greedy sampler implemented",
    "generation_loop: bounded diagnostic loop implemented",
    "generation: unsupported-full-model",
    "inference: not implemented",
    "cuda: available when local driver/device probe succeeds",
    "server: yvexd status shell implemented"};

static const char *const literal_lines_2[] = {"run status: backend-unsupported", "backend: cuda",
                                              "execution_ready: false"};

static const char *const literal_lines_3[] = {"commands:",  "  /help",   "  /status", "  /model",
                                              "  /backend", "  /tokens", "  /reset",  "  /quit"};

typedef struct {
    const char *flag;
    const char **text;
    int *toggle;
    const char *missing_message;
} runtime_option_binding;

/* Bind one runtime option without changing command-specific validation or output. */
/* Purpose: Compute bind runtime option for its CLI invariant (`bind_runtime_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int bind_runtime_option(int arg_count, char **args, int *index,
                               const runtime_option_binding *bindings, size_t binding_count,
                               int *handled) {
    size_t i;

    *handled = 0;
    for (i = 0; i < binding_count; ++i) {
        const runtime_option_binding *binding = &bindings[i];

        if (strcmp(args[*index], binding->flag) != 0)
            continue;
        *handled = 1;
        if (binding->toggle) {
            *binding->toggle = 1;
            return 0;
        }
        if (*index + 1 >= arg_count) {
            if (binding->missing_message) {
                yvex_cli_out_writef(stderr, "%s\n", binding->missing_message);
            } else {
                yvex_cli_out_writef(stderr, "yvex: %s requires a value\n", binding->flag);
            }
            return 2;
        }
        *binding->text = args[++(*index)];
        return 0;
    }
    return 0;
}

/* Purpose: parse the common runtime option grammar while preserving command-specific diagnostics.
 * Inputs: Borrowed argv, option bindings, command name, and context destination.
 * Effects: Updates only destinations declared by the supplied bindings.
 * Failure: Returns exit status 2 after emitting the established option diagnostic.
 * Boundary: Does not validate command-specific required values or capability policy. */
static int parse_runtime_options(int arg_count, char **args, int first,
                                 const runtime_option_binding *bindings, size_t binding_count,
                                 const char *command, unsigned long long *context_length) {
    int i;

    for (i = first; i < arg_count; ++i) {
        int handled = 0;
        int rc = bind_runtime_option(arg_count, args, &i, bindings, binding_count, &handled);

        if (rc != 0)
            return rc;
        if (handled)
            continue;
        if (strcmp(args[i], "--ctx") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            ++i;
            continue;
        }
        yvex_cli_out_writef(stderr, "yvex: unknown %s option: %s\n", command, args[i]);
        yvex_cli_out_writef(stderr, "Try 'yvex help %s' for usage.\n", command);
        return 2;
    }
    return 0;
}

/* Purpose: Render runtime print cuda backend unsupported from typed facts
 *   (`runtime_print_cuda_backend_unsupported`). */
static void runtime_print_cuda_backend_unsupported(const char *status, const yvex_error *err) {
    yvex_cli_out_lines(stdout, literal_pair_6, sizeof(literal_pair_6) / sizeof(literal_pair_6[0]));
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(err));
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "backend-unsupported");
}

/* Purpose: Compute backend name is cuda for its CLI invariant (`backend_name_is_cuda`). */
static int backend_name_is_cuda(const char *name) {
    return name && strcmp(name, "cuda") == 0;
}

/* Purpose: Compute run json string for its CLI invariant (`run_json_string`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void run_json_string(FILE *fp, const char *text) {
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

/* Purpose: Render chat status render from typed facts (`chat_status_render`). */
static int chat_status_render(FILE *fp, const yvex_chat_runtime *runtime, yvex_error *err) {
    yvex_session_summary summary;
    int rc;

    if (!fp || !runtime)
        return YVEX_ERR_INVALID_ARG;
    rc = yvex_chat_runtime_get_summary(runtime, &summary, err);
    if (rc != YVEX_OK)
        return rc;
    yvex_cli_out_writef(fp, "session_state: %s\n", yvex_session_state_name(summary.state));
    yvex_cli_out_writef(fp, "position: %llu\n", summary.position);
    yvex_cli_out_writef(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
    yvex_cli_out_lines(fp, literal_pair_5, sizeof(literal_pair_5) / sizeof(literal_pair_5[0]));
    return YVEX_OK;
}

/* Purpose: render an accepted run result without reclassifying runtime truth.
 * Inputs: writable CLI stream and immutable typed result.
 * Effects: writes plain operator output only.
 * Failure: rejects absent inputs; stream failures remain CLI-I/O owned.
 * Boundary: rendering cannot promote generation readiness. */
static int run_render_plain(FILE *fp, const yvex_chat_accept_result *result) {
    if (!fp || !result)
        return YVEX_ERR_INVALID_ARG;
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
    if (result->run_id[0])
        yvex_cli_out_writef(fp, "run_id: %s\n", result->run_id);
    if (result->run_dir[0])
        yvex_cli_out_writef(fp, "run_dir: %s\n", result->run_dir);
    if (result->metrics_out[0])
        yvex_cli_out_writef(fp, "metrics_out: %s\n", result->metrics_out);
    if (result->trace_out[0])
        yvex_cli_out_writef(fp, "trace_out: %s\n", result->trace_out);
    if (result->profile_out[0])
        yvex_cli_out_writef(fp, "profile_out: %s\n", result->profile_out);
    return YVEX_OK;
}

/* Purpose: Render run render json from typed facts (`run_render_json`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int run_render_json(FILE *fp, const yvex_chat_accept_result *result) {
    if (!fp || !result)
        return YVEX_ERR_INVALID_ARG;
    yvex_cli_out_lines(fp, literal_pair_4, sizeof(literal_pair_4) / sizeof(literal_pair_4[0]));
    yvex_cli_out_writef(fp, "    \"model\": ");
    run_json_string(fp, result->model_name);
    yvex_cli_out_writef(fp, ",\n    \"backend\": ");
    run_json_string(fp, result->backend_name);
    yvex_cli_out_writef(fp, ",\n    \"session_state\": ");
    run_json_string(fp, result->session_state);
    yvex_cli_out_writef(fp, ",\n    \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    yvex_cli_out_writef(fp, "    \"accepted_tokens\": %llu,\n", result->accepted_tokens);
    yvex_cli_out_writef(
        fp, "    \"position\": %llu,\n    \"execution_ready\": false,\n    \"generation\": ",
        result->position);
    run_json_string(fp, result->generation);
    yvex_cli_out_writef(fp, ",\n    \"reason\": ");
    run_json_string(fp, result->reason);
    yvex_cli_out_writef(fp,
                        ",\n    \"metrics\": {\n      \"prompt_tokens\": %llu,\n      "
                        "\"accepted_tokens\": %llu\n    }",
                        result->prompt_tokens, result->accepted_tokens);
    if (result->metrics_out[0] || result->trace_out[0] || result->profile_out[0] ||
        result->run_dir[0]) {
        yvex_cli_out_writef(fp, ",\n    \"artifacts\": {\n      \"run_id\": ");
        run_json_string(fp, result->run_id);
        yvex_cli_out_writef(fp, ",\n      \"run_dir\": ");
        run_json_string(fp, result->run_dir);
        yvex_cli_out_writef(fp, ",\n      \"metrics_out\": ");
        run_json_string(fp, result->metrics_out);
        yvex_cli_out_writef(fp, ",\n      \"trace_out\": ");
        run_json_string(fp, result->trace_out);
        yvex_cli_out_writef(fp, ",\n      \"profile_out\": ");
        run_json_string(fp, result->profile_out);
        yvex_cli_out_writef(fp, "\n    }");
    }
    yvex_cli_out_writef(fp, "\n  },\n  \"error\": null\n}\n");
    return YVEX_OK;
}

/* Purpose: Render status line render from typed facts (`status_line_render`). */
static int status_line_render(FILE *fp, const char *phase, unsigned long long tokens,
                              unsigned long long position) {
    if (!fp || !phase)
        return YVEX_ERR_INVALID_ARG;
    return yvex_cli_out_writef(fp, "[%s] tokens=%llu position=%llu\n", phase, tokens, position) < 0
               ? YVEX_ERR_IO
               : YVEX_OK;
}

/* Shared runtime command helpers. */

/* Purpose: Render print error from typed facts (`print_yvex_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_yvex_error(const yvex_error *err, int exit_code) {
    yvex_cli_out_writef(stderr, "yvex: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

/* Purpose: Compute exit for status for its CLI invariant (`exit_for_status`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int exit_for_status(int status) {
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

/* Purpose: Render print quoted bytes from typed facts (`print_quoted_bytes`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_quoted_bytes(const char *data, unsigned long long len) {
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

/* Purpose: Construct the owned open artifact for gguf state (`open_artifact_for_gguf`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err) {
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

/* Purpose: Render print tensor dims from typed facts (`print_tensor_dims`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_tensor_dims(const unsigned long long *dims, unsigned int rank) {
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

/* Purpose: Render print native dims from typed facts (`print_native_dims`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_native_dims(const unsigned long long *dims, unsigned int rank) {
    print_tensor_dims(dims, rank);
}

/* Purpose: Render print token ids from typed facts (`print_token_ids`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_token_ids(const yvex_tokens *tokens) {
    unsigned long long i;

    yvex_cli_out_writef(stdout, "ids:");
    for (i = 0; i < tokens->len; ++i) {
        yvex_cli_out_writef(stdout, " %u", tokens->ids[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
}

/* Purpose: Parse parse id list into typed CLI state (`parse_id_list`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len) {
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

/* Purpose: Parse parse positive ull into typed CLI state (`parse_positive_ull`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_positive_ull(const char *text, unsigned long long *out) {
    return parse_ull_allow_zero(text, out) && *out != 0;
}

/* Purpose: Parse parse ull allow zero into typed CLI state (`parse_ull_allow_zero`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_ull_allow_zero(const char *text, unsigned long long *out) {
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') {
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

/* Purpose: Parse parse uint allow zero into typed CLI state (`parse_uint_allow_zero`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_uint_allow_zero(const char *text, unsigned int *out) {
    unsigned long long value;

    if (!out || !parse_ull_allow_zero(text, &value) || value > 0xffffffffull)
        return 0;
    *out = (unsigned int)value;
    return 1;
}

/* Purpose: Parse parse dims csv into typed CLI state (`parse_dims_csv`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_dims_csv(const char *text, unsigned int rank, unsigned long long dims[4]) {
    const char *p = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u)
        return 0;
    for (i = 0; i < 4u; ++i)
        dims[i] = 0;
    for (i = 0; i < rank; ++i) {
        unsigned long long value;
        errno = 0;
        value = strtoull(p, &end, 10);
        if (errno != 0 || end == p || value == 0)
            return 0;
        dims[i] = value;
        if (i + 1u < rank) {
            if (*end != ',')
                return 0;
            p = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
    return 1;
}

/* Token input and runtime command surfaces. */

/* Purpose: Orchestrate the typed command engine request (`command_engine`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_engine(int arg_count, char **args) {
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
            yvex_cli_out_writef(stderr, "Try 'yvex help engine' for usage.\n");
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
        yvex_cli_out_writef(stderr,
                            "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n");
        return 2;
    }
    if (backend_name && !cli_backend_name_valid(backend_name)) {
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
    yvex_cli_out_writef(stdout, "unsupported_tensor_accounting: %llu\n",
                        summary.unsupported_tensor_accounting);
    yvex_cli_out_writef(stdout, "tokenizer_model: %s\n", summary.tokenizer_model);
    yvex_cli_out_writef(stdout, "tokenizer_support: %s\n", summary.tokenizer_support);
    yvex_cli_out_writef(stdout, "graph_status: %s\n", summary.graph_status);
    yvex_cli_out_writef(stdout, "weights_attached: %s\n",
                        summary.weights_attached ? "true" : "false");
    yvex_cli_out_writef(stdout, "weights_backend: %s\n", summary.weights_backend);
    yvex_cli_out_writef(stdout, "weight_tensor_count: %llu\n", summary.weight_tensor_count);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", summary.weight_total_bytes);
    yvex_cli_out_writef(stdout, "weight_backend_allocated_bytes: %llu\n",
                        summary.weight_backend_allocated_bytes);
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
            yvex_cli_out_writef(stdout, "attached_weight_%llu: %s role=%s rank=%u dims=[", j,
                                tensor->name ? tensor->name : "",
                                yvex_tensor_role_name(tensor->role), tensor->rank);
            for (d = 0; d < tensor->rank; ++d) {
                if (d > 0) {
                    yvex_cli_out_writef(stdout, ",");
                }
                yvex_cli_out_writef(stdout, "%llu", tensor->dims[d]);
            }
            yvex_cli_out_writef(stdout, "] dtype=%s bytes=%llu\n", yvex_dtype_name(tensor->dtype),
                                tensor->storage_bytes);
        }
    }
    yvex_cli_out_lines(stdout, literal_pair_3, sizeof(literal_pair_3) / sizeof(literal_pair_3[0]));
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_engine_diagnostic_reason(engine));
    yvex_cli_out_writef(stdout, "status: %s\n",
                        summary.weights_attached ? "engine-weights-attached" : "engine-descriptor");

    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

typedef enum { YVEX_INFO_OUTPUT_NORMAL = 0, YVEX_INFO_OUTPUT_AUDIT } yvex_info_output_mode;

/* Purpose: Parse parse info output mode into typed CLI state (`parse_info_output_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int parse_info_output_mode(const char *value, yvex_info_output_mode *mode) {
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

/* Purpose: Render print info normal from typed facts (`print_info_normal`). */
static void print_info_normal(void) {
    yvex_cli_out_writef(stdout, "info: YVEX %s\n", yvex_version_string());
    yvex_cli_out_lines(stdout, literal_lines_0,
                       sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(stdout, "hint: use --audit for full diagnostic fields\n");
}

/* Purpose: Render print info audit from typed facts (`print_info_audit`). */
static void print_info_audit(void) {
    yvex_cli_out_writef(stdout, "name: YVEX\n");
    yvex_cli_out_writef(stdout, "version: %s\n", yvex_version_string());
    yvex_cli_out_lines(stdout, literal_lines_1,
                       sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
}

/* Purpose: Orchestrate the typed command info request (`command_info`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_info(int arg_count, char **args) {
    yvex_info_output_mode output_mode = YVEX_INFO_OUTPUT_NORMAL;
    int i;

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_runtime_info_help(stdout);
            return 0;
        }
        if (strcmp(args[i], "--audit") == 0) {
            output_mode = YVEX_INFO_OUTPUT_AUDIT;
            continue;
        }
        if (strcmp(args[i], "--output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex info: --output requires normal|audit\n");
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

/* Purpose: Orchestrate the typed command plan request (`command_plan`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_plan(int arg_count, char **args) {
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
            yvex_cli_out_writef(stderr, "Try 'yvex help plan' for usage.\n");
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

/* Purpose: Compute trim line for its CLI invariant (`trim_line`). */
static void trim_line(char *line) {
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

/* Purpose: Compute copy text for its CLI invariant (`cli_copy_text`). */
static void cli_copy_text(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src ? src : "");
    dst[cap - 1u] = '\0';
}

/* Purpose: Compute set result artifacts for its CLI invariant (`cli_set_result_artifacts`). */
static void cli_set_result_artifacts(yvex_chat_accept_result *result,
                                     const yvex_run_artifacts *artifacts) {
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

/* Purpose: Transfer bounded write observability files data (`cli_write_observability_files`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int cli_write_observability_files(const char *command_name, const char *model_name,
                                         const char *backend_name, const char *status,
                                         const yvex_run_artifacts *artifacts,
                                         const yvex_metrics *metrics, int arg_count, char **args,
                                         yvex_error *err) {
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

/* Open the shared metrics/trace ownership used by accepted-only run surfaces. */
/* Purpose: Construct the owned observers open state (`cli_observers_open`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int cli_observers_open(yvex_metrics **metrics, yvex_trace **trace,
                              yvex_run_artifacts *artifacts, int save_run, const char *run_dir,
                              const char *metrics_out, const char *trace_out,
                              const char *profile_out, yvex_error *err) {
    yvex_trace_options options;
    int rc;

    rc = yvex_metrics_create(metrics, err);
    if (rc != YVEX_OK)
        return rc;
    rc = yvex_run_artifacts_prepare(artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(*metrics);
        *metrics = NULL;
        return rc;
    }
    memset(&options, 0, sizeof(options));
    options.path = artifacts->has_trace ? artifacts->trace_path : NULL;
    options.run_id = artifacts->run_id;
    options.enabled = artifacts->has_trace;
    rc = yvex_trace_open(trace, &options, err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(*metrics);
        *metrics = NULL;
    }
    return rc;
}

/* Render the accepted-only result without altering runtime ownership. */
/* Purpose: Render run render result from typed facts (`run_render_result`). */
static void run_render_result(const yvex_chat_accept_result *result, const char *output,
                              const char *status_line) {
    if (strcmp(status_line, "always") == 0) {
        (void)status_line_render(stderr, "accept", result->prompt_tokens, result->position);
    }
    if (strcmp(output, "json") == 0) {
        (void)run_render_json(stdout, result);
    } else {
        (void)run_render_plain(stdout, result);
    }
}

/* Publish a typed CUDA admission refusal and release every opened observer. */
/* Purpose: Compute run backend refusal for its CLI invariant (`run_backend_refusal`). */
static int run_backend_refusal(yvex_trace *trace, yvex_error *err) {
    yvex_cli_out_lines(stdout, literal_lines_2,
                       sizeof(literal_lines_2) / sizeof(literal_lines_2[0]));
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(err));
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "backend-unsupported",
                          yvex_error_message(err), err);
    return 5;
}

/* Accepted-only run diagnostics. */

/* Purpose: Orchestrate the typed command run request (`command_run`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_run(int arg_count, char **args) {
    yvex_chat_runtime runtime;
    yvex_chat_accept_result result;
    yvex_engine_summary engine_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
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
    int runtime_open = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (arg_count == 2 ||
        (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0))) {
        yvex_run_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    {
        const runtime_option_binding bindings[] = {
            {"--model", &model_path, NULL, NULL},
            {"--backend", &backend_name, NULL, NULL},
            {"--prompt", &prompt_text, NULL, NULL},
            {"--system", &system_text, NULL, NULL},
            {"--output", &output, NULL, "yvex: --output requires plain or json"},
            {"--status-line", &status_line, NULL,
             "yvex: --status-line requires auto, off, or always"},
            {"--metrics-out", &metrics_out, NULL, NULL},
            {"--trace-out", &trace_out, NULL, NULL},
            {"--profile-out", &profile_out, NULL, NULL},
            {"--run-dir", &run_dir, NULL, NULL},
            {"--save-run", NULL, &save_run, NULL},
        };

        for (i = 2; i < arg_count; ++i) {
            const char *flag = args[i];
            int handled = 0;

            rc = bind_runtime_option(arg_count, args, &i, bindings,
                                     sizeof(bindings) / sizeof(bindings[0]), &handled);
            if (rc != 0)
                return rc;
            if (handled) {
                if (strcmp(flag, "--output") == 0 && strcmp(output, "plain") != 0 &&
                    strcmp(output, "json") != 0) {
                    yvex_cli_out_writef(stderr, "yvex: --output must be plain or json\n");
                    return 2;
                }
                if (strcmp(flag, "--status-line") == 0 && strcmp(status_line, "auto") != 0 &&
                    strcmp(status_line, "off") != 0 && strcmp(status_line, "always") != 0) {
                    yvex_cli_out_writef(stderr,
                                        "yvex: --status-line requires auto, off, or always\n");
                    return 2;
                }
                continue;
            }
            if (strcmp(flag, "--ctx") == 0) {
                if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &context_length)) {
                    yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                    return 2;
                }
                i += 1;
            } else {
                yvex_cli_out_writef(stderr, "yvex: unknown run option: %s\n", flag);
                yvex_cli_out_writef(stderr, "Try 'yvex help run' for usage.\n");
                return 2;
            }
        }
    }

    if (!model_path) {
        yvex_cli_out_writef(stderr,
                            "yvex: --model is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!backend_name) {
        yvex_cli_out_writef(stderr,
                            "yvex: --backend is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!prompt_text) {
        yvex_cli_out_writef(stderr,
                            "yvex: --prompt is required for yvex run in diagnostic runtime\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = cli_observers_open(&metrics, &trace, &artifacts, save_run, run_dir, metrics_out, trace_out,
                            profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        final_rc = run_backend_refusal(trace, &err);
        goto cleanup;
    }
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
        goto cleanup;
    }
    runtime_open = 1;

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    if (yvex_engine_get_summary(runtime.engine, &engine_summary, &err) == YVEX_OK) {
        (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                           engine_summary.unsupported_tensor_accounting, &err);
    }

    rc = yvex_chat_runtime_accept_user_text(&runtime, system_text, prompt_text, &result, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
        goto cleanup;
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "accepted-only",
                          "decode runtime is not implemented in diagnostic runtime", &err);
    cli_set_result_artifacts(&result, &artifacts);

    run_render_result(&result, output, status_line);

    yvex_trace_close(trace);
    trace = NULL;
    rc = cli_write_observability_files("run", result.model_name, result.backend_name,
                                       "accepted-only", &artifacts, metrics, arg_count, args, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

cleanup:
    if (runtime_open)
        yvex_chat_runtime_close(&runtime);
    yvex_trace_close(trace);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    return final_rc;
}

/* Purpose: Orchestrate the typed command session request (`command_session`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_session(int arg_count, char **args) {
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
    int report_error = 0;
    int exit_override = 0;
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

    {
        const runtime_option_binding bindings[] = {
            {"--backend", &backend_name, NULL, NULL},
            {"--text", &text, NULL, NULL},
            {"--accept-tokens", NULL, &accept_tokens, NULL},
        };

        rc = parse_runtime_options(arg_count, args, 3, bindings,
                                   sizeof(bindings) / sizeof(bindings[0]), "session",
                                   &session_options.context_length);
        if (rc != 0)
            return rc;
    }

    if (!cli_backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, args[2], NULL, &err);
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
    }
    rc = enforce_registered_identity_cli(&model_ref, "session");
    if (rc != YVEX_OK)
        goto cleanup;

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
        exit_override = 5;
        goto cleanup;
    }
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
    }

    rc = yvex_backend_kind_parse(backend_name, &backend_options.kind, &err);
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
    }
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name_is_cuda(backend_name)) {
        runtime_print_cuda_backend_unsupported("session-backend-unsupported", &err);
        exit_override = 5;
        goto cleanup;
    }
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
    }

    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
    }

    if (text) {
        rc = yvex_tokenize_text(yvex_engine_tokenizer(engine), text, &tokens, &err);
        if (rc != YVEX_OK) {
            report_error = 1;
            goto cleanup;
        }
        tokenized = 1;
        if (accept_tokens) {
            rc = yvex_session_accept_tokens(session, &tokens, &err);
            if (rc != YVEX_OK) {
                report_error = 1;
                goto cleanup;
            }
        }
    }

    rc = yvex_session_get_summary(session, &summary, &err);
    if (rc != YVEX_OK) {
        report_error = 1;
        goto cleanup;
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
    yvex_cli_out_writef(stdout, "weights_attached: %s\n",
                        summary.weights_attached ? "true" : "false");
    yvex_cli_out_writef(stdout, "weights_backend: %s\n", summary.weights_backend);
    yvex_cli_out_writef(stdout, "weight_tensor_count: %llu\n", summary.weight_tensor_count);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", summary.weight_total_bytes);
    yvex_cli_out_lines(stdout, literal_pair_2, sizeof(literal_pair_2) / sizeof(literal_pair_2[0]));
    yvex_cli_out_writef(stdout, "reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        yvex_cli_out_writef(stdout, "tokens: %llu\n", tokens.len);
    }
    yvex_cli_out_writef(stdout, "status: %s\n",
                        accept_tokens ? "session-token-accepted" : "session-created");

cleanup:
    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    if (exit_override)
        return exit_override;
    if (report_error)
        return print_yvex_error(&err, exit_for_status(rc));
    return rc == YVEX_OK ? 0 : exit_for_status(rc);
}

/* Purpose: Render print chat slash help from typed facts (`print_chat_slash_help`). */
static void print_chat_slash_help(FILE *fp) {
    yvex_cli_out_lines(fp, literal_lines_3, sizeof(literal_lines_3) / sizeof(literal_lines_3[0]));
}

/* Purpose: Render print chat model from typed facts (`print_chat_model`). */
static void print_chat_model(FILE *fp, const yvex_chat_runtime *runtime) {
    yvex_engine_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_engine_get_summary(runtime->engine, &summary, &err) == YVEX_OK) {
        yvex_cli_out_writef(fp, "model: %s\n", summary.model_name);
        yvex_cli_out_writef(fp, "architecture: %s\n", summary.architecture);
        yvex_cli_out_writef(fp, "tokenizer: %s\n", summary.tokenizer_model);
    }
}

/* Purpose: render admitted backend identity and capabilities for chat diagnostics.
 * Inputs: writable CLI stream and opened chat runtime.
 * Effects: writes plain operator output only.
 * Failure: callers validate both inputs before entry.
 * Boundary: capability projection does not execute generation. */
static void print_chat_backend(FILE *fp, const yvex_chat_runtime *runtime) {
    yvex_cli_out_writef(fp, "backend: %s\n",
                        yvex_backend_kind_name(yvex_backend_kind_of(runtime->backend)));
    yvex_cli_out_writef(fp, "status: %s\n",
                        yvex_backend_status_name(yvex_backend_status_of(runtime->backend)));
    yvex_cli_out_writef(fp, "capabilities:\n");
    yvex_cli_out_writef(
        fp, "  tensor_alloc: %s\n",
        yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ? "yes" : "no");
    yvex_cli_out_writef(
        fp, "  tensor_read_write: %s\n",
        yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE) ? "yes" : "no");
    yvex_cli_out_writef(fp, "  op_embed: %s\n",
                        yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_EMBED) ? "yes"
                                                                                           : "no");
    yvex_cli_out_writef(fp, "  op_mlp: %s\n",
                        yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_MLP) ? "yes"
                                                                                         : "no");
    yvex_cli_out_writef(fp, "  op_rope: %s\n",
                        yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_ROPE) ? "yes"
                                                                                          : "no");
}

/* Purpose: Render print chat tokens from typed facts (`print_chat_tokens`). */
static void print_chat_tokens(FILE *fp, const yvex_chat_runtime *runtime) {
    yvex_session_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_chat_runtime_get_summary(runtime, &summary, &err) == YVEX_OK) {
        yvex_cli_out_writef(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
        yvex_cli_out_writef(fp, "position: %llu\n", summary.position);
    }
}

/* Purpose: Compute handle chat slash for its CLI invariant (`handle_chat_slash`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int handle_chat_slash(yvex_chat_runtime *runtime, yvex_slash_command command,
                             const char *line, int *done, yvex_error *err) {
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
        yvex_cli_out_lines(stdout, literal_pair_1,
                           sizeof(literal_pair_1) / sizeof(literal_pair_1[0]));
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

/* Purpose: Construct the owned resolve chat model ref state (`resolve_chat_model_ref`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int resolve_chat_model_ref(yvex_model_ref *out, const char *explicit_model,
                                  yvex_error *err) {
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
            yvex_error_set(
                err, YVEX_ERR_INVALID_ARG, "chat",
                "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
            return YVEX_ERR_INVALID_ARG;
        }
        return rc;
    }

    selected = yvex_model_registry_selected(registry);
    if (!selected || !selected->alias || !selected->alias[0]) {
        yvex_model_registry_close(registry);
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "chat",
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

/* Process accepted-only REPL turns while the caller retains runtime ownership. */
/* Purpose: Orchestrate the typed command chat loop request (`command_chat_loop`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_chat_loop(yvex_chat_runtime *runtime, yvex_metrics *metrics, yvex_trace *trace,
                             yvex_error *err) {
    unsigned long long phase_token;
    char line[4096];
    int done = 0;
    int rc;

    while (!done) {
        yvex_chat_accept_result result;

        yvex_cli_out_writef(stdout, "yvex> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        trim_line(line);
        if (line[0] == '\0')
            continue;
        if (line[0] == '/') {
            rc = handle_chat_slash(runtime, yvex_slash_parse(line), line, &done, err);
            if (rc != YVEX_OK)
                return rc;
            continue;
        }
        rc = yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_CHAT_TURN, &phase_token, err);
        if (rc != YVEX_OK)
            return rc;
        rc = yvex_chat_runtime_accept_user_text(runtime, NULL, line, &result, err);
        (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_CHAT_TURN, phase_token, err);
        if (rc != YVEX_OK)
            return rc;
        (void)yvex_metrics_add_chat_turn(metrics, err);
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_CHAT_TURN, "chat_turn", "accepted",
                              "user prompt accepted", err);
        yvex_cli_out_writef(stdout, "accepted tokens: %llu\n", result.prompt_tokens);
        yvex_cli_out_writef(stdout, "position: %llu\n", result.position);
        yvex_cli_out_writef(stdout, "assistant: [generation unsupported in diagnostic runtime]\n");
    }
    return YVEX_OK;
}

/* Accepted-only diagnostic REPL command surface. */

/* Purpose: Orchestrate the typed command chat request (`command_chat`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_chat(int arg_count, char **args) {
    yvex_chat_runtime runtime;
    yvex_engine_summary engine_summary;
    yvex_session_summary session_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
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
    int save_run = 0;
    int runtime_open = 0;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (arg_count == 2 ||
        (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0))) {
        yvex_chat_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    {
        const runtime_option_binding bindings[] = {
            {"--model", &model_path, NULL, NULL},        {"--backend", &backend_name, NULL, NULL},
            {"--metrics-out", &metrics_out, NULL, NULL}, {"--trace-out", &trace_out, NULL, NULL},
            {"--profile-out", &profile_out, NULL, NULL}, {"--run-dir", &run_dir, NULL, NULL},
            {"--save-run", NULL, &save_run, NULL},
        };

        rc = parse_runtime_options(arg_count, args, 2, bindings,
                                   sizeof(bindings) / sizeof(bindings[0]), "chat", &context_length);
        if (rc != 0)
            return rc;
    }

    if (!backend_name) {
        yvex_cli_out_writef(stderr,
                            "yvex: --backend is required for yvex chat in diagnostic runtime\n");
        return 2;
    }

    rc = resolve_chat_model_ref(&model_ref, model_path, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = cli_observers_open(&metrics, &trace, &artifacts, save_run, run_dir, metrics_out, trace_out,
                            profile_out, &err);
    if (rc != YVEX_OK) {
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
        final_rc = 5;
        goto cleanup;
    }
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
        goto cleanup;
    }
    runtime_open = 1;

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    (void)yvex_engine_get_summary(runtime.engine, &engine_summary, &err);
    (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                       engine_summary.unsupported_tensor_accounting, &err);
    (void)yvex_chat_runtime_get_summary(&runtime, &session_summary, &err);
    yvex_cli_out_writef(stdout, "YVEX chat runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", engine_summary.model_name);
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "session_state: %s\n",
                        yvex_session_state_name(session_summary.state));
    yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));

    rc = command_chat_loop(&runtime, metrics, trace, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
        goto cleanup;
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "accepted-only",
                          "chat runtime exited without generation", &err);
    yvex_trace_close(trace);
    trace = NULL;
    rc = cli_write_observability_files("chat", engine_summary.model_name, backend_name,
                                       "accepted-only", &artifacts, metrics, arg_count, args, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

cleanup:
    if (runtime_open)
        yvex_chat_runtime_close(&runtime);
    yvex_trace_close(trace);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    return final_rc;
}

/* Purpose: Orchestrate the typed chat command request (`yvex_chat_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_chat_command(int arg_count, char **args) {
    return command_chat(arg_count, args);
}

/* Purpose: dispatch engine diagnostic CLI requests to the existing runtime command implementation.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_engine_command(int arg_count, char **args) {
    return command_engine(arg_count, args);
}

/* Purpose: Orchestrate the typed runtime info command request (`yvex_runtime_info_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_runtime_info_command(int arg_count, char **args) {
    return command_info(arg_count, args);
}

/* Purpose: Orchestrate the typed plan command request (`yvex_plan_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_plan_command(int arg_count, char **args) {
    return command_plan(arg_count, args);
}

/* Purpose: Orchestrate the typed run command request (`yvex_run_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_run_command(int arg_count, char **args) {
    return command_run(arg_count, args);
}

/* Purpose: Orchestrate the typed session command request (`yvex_session_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_session_command(int arg_count, char **args) {
    return command_session(arg_count, args);
}

/* Purpose: Render chat help from typed facts (`yvex_chat_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_chat_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex chat [--model FILE_OR_ALIAS] --backend cpu|cuda [--ctx N] "
                            "[--metrics-out FILE] [--"
                            "trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir "
                            "DIR]\n\nChat opens diagnostic engine/"
                            "backend/session state and accepts user text without generating "
                            "output. If --model is omitted, "
                            "chat uses the current model selected with yvex models use ALIAS.\n");
}

/* Purpose: Render engine help from typed facts (`yvex_engine_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_engine_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex engine [--model] FILE_OR_ALIAS [--backend "
                            "cpu|cuda]\n\nEngine opens descriptor/"
                            "tokenizer/graph state and can observe selected materialized "
                            "residency. It does not execute "
                            "prefill, decode, or generation.\n");
}

/* Purpose: Render runtime info help from typed facts (`yvex_runtime_info_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_runtime_info_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex info [--audit | --output normal|audit]\n\nPrints the "
                            "implemented build and runtime "
                            "boundary status. Normal output is compact; audit output preserves "
                            "full diagnostic fields.\n");
}

/* Purpose: Render plan help from typed facts (`yvex_plan_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_plan_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx "
                            "N]\n\nPlan builds graph and memory "
                            "estimates. Execution remains disabled.\n");
}

/* Purpose: Render run help from typed facts (`yvex_run_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_run_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex run --model FILE --backend cpu|cuda --prompt TEXT "
                            "[--system TEXT] [--output plain|"
                            "json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] "
                            "[--save-run] [--run-dir DIR]\n\n"
                            "Run accepts one prompt through the diagnostic runtime path and "
                            "reports accepted-only diagnostics.\n"
                            "");
}

/* Purpose: Render session help from typed facts (`yvex_session_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_session_help(FILE *fp) {
    yvex_cli_out_writef(fp, "usage: yvex session FILE_OR_ALIAS [--backend cpu|cuda] [--ctx N] "
                            "[--text TEXT] [--accept-tokens]\n"
                            "\nSession creates a lifecycle diagnostic session over engine/backend "
                            "state. It does not run "
                            "prefill, decode, or generation.\n");
}
