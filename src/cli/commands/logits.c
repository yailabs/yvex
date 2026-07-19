/* Owner: CLI logits command.
 * Owns: logits argv validation, dispatch, help, and compatibility rendering.
 * Does not own: logits buffer semantics, decode, sampling, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes the bounded logits API and returns process exit status.
 * Purpose: provide logits argv validation, dispatch, help, and compatibility rendering.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"
#include <yvex/generation.h>
#include <yvex/runtime.h>
#include <yvex/core.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const literal_pair_0[] = { "logits: buffer",
    "status: logits-buffer"};

static const char *const literal_lines_0[] = { "real_model_logits: false",
    "real_model_output_head: false",
    "logits_ready: false",
    "sampling_ready: false",
    "generation_ready: false",
    "generation: unsupported"};

/* Purpose: Render logits print summary from typed facts (`logits_print_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void logits_print_summary(const yvex_logits_buffer_summary *summary,
                                 const char *model_arg,
                                 const char *backend_name,
                                 const char *segment_name,
                                 const char *token_input_status,
                                 unsigned long long input_token_count,
                                 const char *status)
{
    unsigned long long i;

    yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n",
           summary && summary->backend_name ? summary->backend_name
                                            : (backend_name ? backend_name : "cpu"));
    yvex_cli_out_writef(stdout, "segment: %s\n", segment_name ? segment_name : "embedding-rmsnorm");
    yvex_cli_out_writef(stdout, "token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    yvex_cli_out_writef(stdout, "input_token_count: %llu\n", input_token_count);
    yvex_cli_out_writef(stdout, "decode_invoked: %s\n", summary && summary->decode_invoked ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_state_created: %s\n",
           summary && summary->decode_state_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_executed: %s\n",
           summary && summary->decode_step_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_kind: %s\n",
           summary && summary->decode_step_kind ? summary->decode_step_kind : "none");
    yvex_cli_out_writef(stdout, "decode_phase: %s\n",
           summary && summary->decode_phase ? summary->decode_phase : "not-started");
    yvex_cli_out_writef(stdout, "decode_position: %llu\n", summary ? summary->decode_position : 0ull);
    yvex_cli_out_writef(stdout, "decode_state_checksum: %llu\n",
           summary ? summary->decode_state_checksum : 0ull);
    yvex_cli_out_writef(stdout, "logits_buffer_created: %s\n",
           summary && summary->logits_buffer_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "logits_buffer_kind: %s\n",
           summary && summary->logits_buffer_kind ? summary->logits_buffer_kind
                                                  : "bounded-diagnostic");
    yvex_cli_out_writef(stdout, "logits_phase: %s\n",
           summary && summary->logits_phase ? summary->logits_phase : "preflight");
    yvex_cli_out_writef(stdout, "logits_source: %s\n",
           summary && summary->logits_source ? summary->logits_source : "decode-state");
    yvex_cli_out_writef(stdout, "logits_count: %llu\n", summary ? summary->logits_count : 0ull);
    yvex_cli_out_writef(stdout, "logits_bytes: %llu\n", summary ? summary->logits_bytes : 0ull);
    yvex_cli_out_writef(stdout, "logits_checksum: %llu\n", summary ? summary->logits_checksum : 0ull);
    yvex_cli_out_writef(stdout, "logits_min: %.9g\n", summary ? summary->logits_min : 0.0);
    yvex_cli_out_writef(stdout, "logits_max: %.9g\n", summary ? summary->logits_max : 0.0);
    yvex_cli_out_writef(stdout, "logits_sum: %.9g\n", summary ? summary->logits_sum : 0.0);
    yvex_cli_out_writef(stdout, "logits_sample_count: %llu\n",
           summary ? summary->logits_sample_count : 0ull);
    if (summary) {
        for (i = 0; i < summary->logits_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "logit_%llu: %.9g\n", i, (double)summary->logits_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "bounded_logits_ready: %s\n",
           summary && summary->bounded_logits_ready ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "logits-buffer-fail");
}

/* Purpose: Compute logits defaults for its CLI invariant (`logits_cli_defaults`). */
static void logits_cli_defaults(yvex_logits_buffer_summary *summary,
                                const char *backend_name)
{
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options options;

    memset(&decode_options, 0, sizeof(decode_options));
    memset(&options, 0, sizeof(options));
    decode_options.backend_name = backend_name ? backend_name : "cpu";
    decode_options.segment_name = "embedding-rmsnorm";
    options.decode_options = &decode_options;
    options.logits_count = 16ull;
    yvex_logits_buffer_summary_init(summary, &options);
}

typedef struct {
    const char *model;
    const char *backend;
    const char *segment;
    const char *tokens;
    yvex_kv_shape kv_shape;
    unsigned long long vocab_size;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
    unsigned long long chunk_size;
    unsigned long long position_start;
    unsigned long long context_length;
    unsigned long long logits_count;
    int attach_kv;
    int kv_shape_seen;
    int layer_count_seen;
    int layer_hidden_seen;
    int layer_head_seen;
    int layer_ffn_seen;
    int chunk_size_seen;
} logits_cli_options;

/* Parse one numeric option while retaining which optional shape facts were supplied. */
/* Purpose: Parse logits parse number into typed CLI state (`logits_parse_number`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int logits_parse_number(int arg_count,
                               char **args,
                               int *index,
                               logits_cli_options *options)
{
    const char *name = args[*index];
    const char *error = NULL;
    unsigned long long *value = NULL;
    int *seen = NULL;
    int allow_zero = 0;

    if (strcmp(name, "--logits-count") == 0) {
        value = &options->logits_count;
        error = "yvex: --logits-count requires 1 <= N <= 256\n";
    } else if (strcmp(name, "--kv-layers") == 0) {
        value = &options->kv_shape.layer_count;
        seen = &options->kv_shape_seen;
        error = "yvex: --kv-layers requires a positive integer\n";
    } else if (strcmp(name, "--kv-heads") == 0) {
        value = &options->kv_shape.kv_head_count;
        seen = &options->kv_shape_seen;
        error = "yvex: --kv-heads requires a positive integer\n";
    } else if (strcmp(name, "--kv-head-dim") == 0) {
        value = &options->kv_shape.head_dim;
        seen = &options->kv_shape_seen;
        error = "yvex: --kv-head-dim requires a positive integer\n";
    } else if (strcmp(name, "--kv-capacity") == 0) {
        value = &options->kv_shape.capacity;
        seen = &options->kv_shape_seen;
        error = "yvex: --kv-capacity requires a positive integer\n";
    } else if (strcmp(name, "--layers") == 0) {
        value = &options->layer_count;
        seen = &options->layer_count_seen;
        error = "yvex: --layers requires a positive integer\n";
    } else if (strcmp(name, "--layer-hidden-dim") == 0) {
        value = &options->layer_hidden_dim;
        seen = &options->layer_hidden_seen;
        error = "yvex: --layer-hidden-dim requires a positive integer\n";
    } else if (strcmp(name, "--layer-head-dim") == 0) {
        value = &options->layer_head_dim;
        seen = &options->layer_head_seen;
        error = "yvex: --layer-head-dim requires a positive integer\n";
    } else if (strcmp(name, "--layer-ffn-dim") == 0) {
        value = &options->layer_ffn_dim;
        seen = &options->layer_ffn_seen;
        error = "yvex: --layer-ffn-dim requires a positive integer\n";
    } else if (strcmp(name, "--chunk-size") == 0) {
        value = &options->chunk_size;
        seen = &options->chunk_size_seen;
        error = "yvex: --chunk-size requires a positive integer\n";
    } else if (strcmp(name, "--position-start") == 0) {
        value = &options->position_start;
        allow_zero = 1;
        error = "yvex: --position-start requires a non-negative integer\n";
    } else if (strcmp(name, "--context-length") == 0) {
        value = &options->context_length;
        error = "yvex: --context-length requires a positive integer\n";
    } else {
        return 0;
    }
    if (*index + 1 >= arg_count ||
        !(allow_zero ? parse_ull_allow_zero(args[*index + 1], value)
                     : parse_positive_ull(args[*index + 1], value)) ||
        (strcmp(name, "--logits-count") == 0 && !yvex_logits_count_valid(*value))) {
        yvex_cli_out_writef(stderr, "%s", error);
        return -1;
    }
    if (seen) *seen = 1;
    *index += 1;
    return 1;
}

/* Parse the logits surface without opening a model or allocating runtime state. */
/* Purpose: Parse logits options parse into typed CLI state (`logits_options_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int logits_options_parse(int arg_count, char **args, logits_cli_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->logits_count = 16ull;
    for (i = 2; i < arg_count; ++i) {
        int numeric = logits_parse_number(arg_count, args, &i, options);
        if (numeric < 0) return 2;
        if (numeric > 0) continue;
        if (strcmp(args[i], "--attach-kv") == 0) {
            options->attach_kv = 1;
        } else if (strcmp(args[i], "--model") == 0 ||
                   strcmp(args[i], "--backend") == 0 ||
                   strcmp(args[i], "--segment") == 0 ||
                   strcmp(args[i], "--tokens") == 0) {
            const char *message = strcmp(args[i], "--model") == 0
                ? "yvex: --model requires FILE_OR_ALIAS\n"
                : strcmp(args[i], "--backend") == 0
                    ? "yvex: --backend requires cpu|cuda\n"
                    : strcmp(args[i], "--segment") == 0
                        ? "yvex: --segment requires embedding-rmsnorm\n"
                        : "yvex: --tokens requires IDS\n";
            const char **target = strcmp(args[i], "--model") == 0 ? &options->model
                : strcmp(args[i], "--backend") == 0 ? &options->backend
                : strcmp(args[i], "--segment") == 0 ? &options->segment : &options->tokens;
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "%s", message);
                return 2;
            }
            *target = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown logits option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help logits' for usage.\n");
            return 2;
        }
    }
    return 0;
}

/* Validate cross-option constraints and install default layer geometry. */
/* Purpose: Validate logits options validate before downstream use (`logits_options_validate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int logits_options_validate(logits_cli_options *options)
{
    if (!options->model || !options->backend || !options->tokens || !options->segment) {
        yvex_cli_out_writef(stderr,
            "usage: yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --"
                "tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-"
                "size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --"
                "kv-head-dim N --kv-capacity N] [--logits-count N]\n");
        return 2;
    }
    if (!cli_backend_name_valid(options->backend)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", options->backend);
        return 2;
    }
    if (strcmp(options->segment, "embedding-rmsnorm") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unsupported logits segment: %s\n", options->segment);
        return 2;
    }
    if (options->kv_shape_seen && !options->attach_kv) {
        yvex_cli_out_writef(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (options->attach_kv && (options->kv_shape.layer_count == 0ull ||
        options->kv_shape.kv_head_count == 0ull || options->kv_shape.head_dim == 0ull ||
        options->kv_shape.capacity == 0ull)) {
        yvex_cli_out_writef(stderr,
            "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!options->layer_count_seen && (options->layer_hidden_seen ||
        options->layer_head_seen || options->layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (options->layer_count_seen && (options->layer_hidden_seen || options->layer_head_seen ||
        options->layer_ffn_seen) && !(options->layer_hidden_seen && options->layer_head_seen &&
        options->layer_ffn_seen)) {
        yvex_cli_out_writef(stderr,
            "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (options->layer_count_seen && options->layer_count > 16ull) {
        yvex_cli_out_writef(stderr, "yvex: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (options->layer_count_seen && !options->layer_hidden_seen) {
        options->layer_hidden_dim = 8ull;
        options->layer_head_dim = 8ull;
        options->layer_ffn_dim = 16ull;
    }
    if (options->layer_count_seen &&
        options->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_cli_out_writef(stderr,
            "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
            (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }
    return 0;
}

/* Purpose: Orchestrate the typed logits command request (`yvex_logits_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_logits_command(int arg_count, char **args)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_logits_buffer_summary logits_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    logits_cli_options cli;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&logits_summary, 0, sizeof(logits_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_logits_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }
    rc = logits_options_parse(arg_count, args, &cli);
    if (rc != 0) return rc;
    rc = logits_options_validate(&cli);
    if (rc != 0) return rc;

    rc = yvex_model_ref_resolve(&model_ref, cli.model, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "logits");
    if (rc != YVEX_OK) {
        logits_cli_defaults(&logits_summary, cli.backend);
        logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "fail", 0ull,
                             "logits-buffer-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(cli.tokens, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = yvex_model_context_vocab_size(model_ref.path, &cli.vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, cli.vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        logits_cli_defaults(&logits_summary, cli.backend);
        logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "fail",
                             token_input.token_count, "logits-buffer-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_preflight(&model_ref,
                               cli.backend,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        logits_cli_defaults(&logits_summary, cli.backend);
        logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "pass",
                             token_input.token_count, "logits-buffer-fail");
        yvex_cli_graph_guard_print(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = cli.backend;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        logits_cli_defaults(&logits_summary, cli.backend);
        logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "pass",
                             token_input.token_count, "logits-buffer-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    decode_options.token_input = &token_input;
    decode_options.segment_name = cli.segment;
    decode_options.backend_name = cli.backend;
    decode_options.position_start = cli.position_start;
    decode_options.chunk_size = cli.chunk_size_seen ? cli.chunk_size : 0ull;
    decode_options.context_length = cli.context_length;
    decode_options.attach_kv = cli.attach_kv;
    decode_options.kv_shape = cli.kv_shape;
    decode_options.layer_count = cli.layer_count_seen ? cli.layer_count : 0ull;
    decode_options.layer_hidden_dim = cli.layer_hidden_dim;
    decode_options.layer_head_dim = cli.layer_head_dim;
    decode_options.layer_ffn_dim = cli.layer_ffn_dim;
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = cli.logits_count;

    rc = yvex_engine_create_logits_buffer(engine, &logits_options, &logits_summary, &err);
    if (rc != YVEX_OK) {
        const char *status = "logits-buffer-fail";
        if (logits_summary.logits_phase &&
            strcmp(logits_summary.logits_phase, "fill") == 0 &&
            logits_summary.cleanup_attempted &&
            logits_summary.cleanup_status &&
            strcmp(logits_summary.cleanup_status, "pass") == 0) {
            status = "logits-buffer-failed-cleaned";
        }
        logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "pass",
                             token_input.token_count, status);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    logits_print_summary(&logits_summary, cli.model, cli.backend, cli.segment, "pass",
                         token_input.token_count, "logits-buffer-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

/* Purpose: Render logits help from typed facts (`yvex_logits_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_logits_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens "
            "IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--"
            "position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --"
            "kv-capacity N] [--logits-count N]\n\nLogits creates a bounded diagnostic logits buffer from the "
            "implemented decode state. It does not run the real model output head, sample, generate, or claim "
            "DeepSeek logits.\n");
}
