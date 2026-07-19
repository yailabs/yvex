/* Owner: CLI decode command.
 * Owns: decode argv validation, dispatch, help, and compatibility rendering.
 * Does not own: decode state semantics, prefill, KV, logits, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes the bounded decode API and returns process exit status.
 * Purpose: provide decode argv validation, dispatch, help, and compatibility rendering.
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

static const char *const literal_pair_0[] = { "decode: step",
    "status: decode-step"};

static const char *const literal_lines_0[] = { "real_model_decode: false",
    "full_model_decode_ready: false",
    "logits_ready: false",
    "sampling_ready: false",
    "generation_ready: false",
    "generation: unsupported"};

/* Purpose: Render decode print summary from typed facts (`decode_print_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void decode_print_summary(const yvex_decode_step_summary *summary,
                                 const char *model_arg,
                                 const char *backend_name,
                                 const char *token_input_status,
                                 const char *status)
{
    unsigned long long i;

    yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n",
           summary && summary->backend_name ? summary->backend_name
                                            : (backend_name ? backend_name : "cpu"));
    yvex_cli_out_writef(stdout, "segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    yvex_cli_out_writef(stdout, "token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    yvex_cli_out_writef(stdout, "input_token_count: %llu\n", summary ? summary->input_token_count : 0ull);
    yvex_cli_out_writef(stdout, "prefill_invoked: %s\n", summary && summary->prefill_invoked ? "true" : "false");
    yvex_cli_out_writef(stdout, "prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "none");
    yvex_cli_out_writef(stdout, "prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "not-started");
    yvex_cli_out_writef(stdout, "prefill_tokens_processed: %llu\n",
           summary ? summary->prefill_tokens_processed : 0ull);
    yvex_cli_out_writef(stdout, "prefill_position_start: %llu\n",
           summary ? summary->prefill_position_start : 0ull);
    yvex_cli_out_writef(stdout, "prefill_position_end: %llu\n",
           summary ? summary->prefill_position_end : 0ull);
    yvex_cli_out_writef(stdout, "decode_position: %llu\n", summary ? summary->decode_position : 0ull);
    yvex_cli_out_writef(stdout, "context_length: %llu\n", summary ? summary->context_length : 0ull);
    yvex_cli_out_writef(stdout, "context_boundary_status: %s\n",
           summary && summary->context_boundary_status
               ? summary->context_boundary_status
               : "unchecked");
    yvex_cli_out_writef(stdout, "decode_state_created: %s\n",
           summary && summary->decode_state_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_executed: %s\n",
           summary && summary->decode_step_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_kind: %s\n",
           summary && summary->decode_step_kind ? summary->decode_step_kind : "bounded-diagnostic");
    yvex_cli_out_writef(stdout, "decode_phase: %s\n",
           summary && summary->decode_phase ? summary->decode_phase : "preflight");
    yvex_cli_out_writef(stdout, "decode_execution_mode: %s\n",
           summary && summary->decode_execution_mode
               ? summary->decode_execution_mode
               : "prefill-summary-advance");
    yvex_cli_out_writef(stdout, "decode_state_source: %s\n",
           summary && summary->decode_state_source ? summary->decode_state_source : "prefill-aggregate");
    yvex_cli_out_writef(stdout, "decode_state_checksum: %llu\n",
           summary ? summary->decode_state_checksum : 0ull);
    yvex_cli_out_writef(stdout, "decode_state_value_count: %llu\n",
           summary ? summary->decode_state_value_count : 0ull);
    if (summary) {
        for (i = 0; i < summary->decode_state_value_count; ++i) {
            yvex_cli_out_writef(stdout, "decode_state_value_%llu: %.9g\n",
                   i, (double)summary->decode_state_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "prefill_aggregate_checksum: %llu\n",
           summary ? summary->prefill_aggregate_checksum : 0ull);
    yvex_cli_out_writef(stdout, "prefill_final_token_checksum: %llu\n",
           summary ? summary->prefill_final_token_checksum : 0ull);
    yvex_cli_out_writef(stdout, "kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "kv_binding_source: %s\n",
           summary && summary->kv_binding_source ? summary->kv_binding_source : "none");
    yvex_cli_out_writef(stdout, "kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    yvex_cli_out_writef(stdout, "kv_positions_written: %llu\n",
           summary ? summary->kv_positions_written : 0ull);
    yvex_cli_out_writef(stdout, "kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "decode-step-fail");
}

/* Purpose: Compute decode defaults for its CLI invariant (`decode_cli_defaults`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void decode_cli_defaults(yvex_decode_step_summary *summary,
                                const char *backend_name,
                                const char *segment_name)
{
    yvex_decode_step_options options;

    memset(&options, 0, sizeof(options));
    options.backend_name = backend_name ? backend_name : "cpu";
    options.segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    yvex_decode_step_summary_init(summary, &options);
}

typedef struct {
    const char *model;
    const char *backend;
    const char *segment;
    const char *tokens;
    yvex_kv_shape kv_shape;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
    unsigned long long chunk_size;
    unsigned long long position_start;
    unsigned long long context_length;
    int attach_kv;
    int kv_shape_seen;
    int layer_count_seen;
    int layer_hidden_seen;
    int layer_head_seen;
    int layer_ffn_seen;
    int chunk_size_seen;
} decode_cli_options;

/* Parse one decode option and advance the borrowed argument cursor. */
/* Purpose: Parse decode parse option into typed CLI state (`decode_parse_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int decode_parse_option(int argc, char **argv, int *index,
                               decode_cli_options *options)
{
    const char *arg = argv[*index];
    const char *value = *index + 1 < argc ? argv[*index + 1] : NULL;
    unsigned long long *number = NULL;
    int *seen = NULL;

    if (strcmp(arg, "--model") == 0) options->model = value;
    else if (strcmp(arg, "--backend") == 0) options->backend = value;
    else if (strcmp(arg, "--segment") == 0) options->segment = value;
    else if (strcmp(arg, "--tokens") == 0) options->tokens = value;
    else if (strcmp(arg, "--attach-kv") == 0) {
        options->attach_kv = 1;
        return 0;
    } else if (strcmp(arg, "--kv-layers") == 0) {
        number = &options->kv_shape.layer_count;
        seen = &options->kv_shape_seen;
    } else if (strcmp(arg, "--kv-heads") == 0) {
        number = &options->kv_shape.kv_head_count;
        seen = &options->kv_shape_seen;
    } else if (strcmp(arg, "--kv-head-dim") == 0) {
        number = &options->kv_shape.head_dim;
        seen = &options->kv_shape_seen;
    } else if (strcmp(arg, "--kv-capacity") == 0) {
        number = &options->kv_shape.capacity;
        seen = &options->kv_shape_seen;
    } else if (strcmp(arg, "--layers") == 0) {
        number = &options->layer_count;
        seen = &options->layer_count_seen;
    } else if (strcmp(arg, "--layer-hidden-dim") == 0) {
        number = &options->layer_hidden_dim;
        seen = &options->layer_hidden_seen;
    } else if (strcmp(arg, "--layer-head-dim") == 0) {
        number = &options->layer_head_dim;
        seen = &options->layer_head_seen;
    } else if (strcmp(arg, "--layer-ffn-dim") == 0) {
        number = &options->layer_ffn_dim;
        seen = &options->layer_ffn_seen;
    } else if (strcmp(arg, "--chunk-size") == 0) {
        number = &options->chunk_size;
        seen = &options->chunk_size_seen;
    } else if (strcmp(arg, "--position-start") == 0) {
        if (!value || !parse_ull_allow_zero(value, &options->position_start)) {
            yvex_cli_out_writef(stderr,
                                "yvex: --position-start requires a non-negative integer\n");
            return 2;
        }
        *index += 1;
        return 0;
    } else if (strcmp(arg, "--context-length") == 0) {
        number = &options->context_length;
    } else {
        yvex_cli_out_writef(stderr, "yvex: unknown decode option: %s\n", arg);
        yvex_cli_out_writef(stderr, "Try 'yvex help decode' for usage.\n");
        return 2;
    }
    if (!value) {
        yvex_cli_out_writef(stderr, "yvex: %s requires a value\n", arg);
        return 2;
    }
    if (number && !parse_positive_ull(value, number)) {
        yvex_cli_out_writef(stderr, "yvex: %s requires a positive integer\n", arg);
        return 2;
    }
    if (seen) *seen = 1;
    *index += 1;
    return 0;
}

/* Validate option relationships and install the documented layer defaults. */
/* Purpose: Validate decode options validate before downstream use (`decode_options_validate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int decode_options_validate(decode_cli_options *options)
{
    if (!options->model || !options->backend || !options->tokens || !options->segment) {
        yvex_cli_out_writef(stderr,
            "usage: yvex decode --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --"
                "tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-"
                "size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --"
                "kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (!cli_backend_name_valid(options->backend)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", options->backend);
        return 2;
    }
    if (strcmp(options->segment, "embedding-rmsnorm") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unsupported decode segment: %s\n", options->segment);
        return 2;
    }
    if (options->kv_shape_seen && !options->attach_kv) {
        yvex_cli_out_writef(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (options->attach_kv &&
        (!options->kv_shape.layer_count || !options->kv_shape.kv_head_count ||
         !options->kv_shape.head_dim || !options->kv_shape.capacity)) {
        yvex_cli_out_writef(stderr,
            "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!options->layer_count_seen &&
        (options->layer_hidden_seen || options->layer_head_seen || options->layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (options->layer_count_seen &&
        (options->layer_hidden_seen || options->layer_head_seen || options->layer_ffn_seen) &&
        !(options->layer_hidden_seen && options->layer_head_seen && options->layer_ffn_seen)) {
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

/* Purpose: Orchestrate the typed decode command request (`yvex_decode_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_decode_command(int arg_count, char **args)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_decode_step_summary decode_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    decode_cli_options options;
    unsigned long long vocab_size = 0ull;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&decode_summary, 0, sizeof(decode_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&options, 0, sizeof(options));

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_decode_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        rc = decode_parse_option(arg_count, args, &i, &options);
        if (rc != 0) return rc;
    }
    rc = decode_options_validate(&options);
    if (rc != 0) return rc;

    rc = yvex_model_ref_resolve(&model_ref, options.model, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "decode");
    if (rc != YVEX_OK) {
        decode_cli_defaults(&decode_summary, options.backend, options.segment);
        decode_print_summary(&decode_summary, options.model, options.backend,
                             "fail", "decode-step-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(options.tokens, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = yvex_model_context_vocab_size(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        decode_cli_defaults(&decode_summary, options.backend, options.segment);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, options.model, options.backend,
                             "fail", "decode-step-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_preflight(&model_ref,
                               options.backend,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        decode_cli_defaults(&decode_summary, options.backend, options.segment);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, options.model, options.backend,
                             "pass", "decode-step-fail");
        yvex_cli_graph_guard_print(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = options.backend;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        decode_cli_defaults(&decode_summary, options.backend, options.segment);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, options.model, options.backend,
                             "pass", "decode-step-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    decode_options.token_input = &token_input;
    decode_options.segment_name = options.segment;
    decode_options.backend_name = options.backend;
    decode_options.position_start = options.position_start;
    decode_options.chunk_size = options.chunk_size_seen ? options.chunk_size : 0ull;
    decode_options.context_length = options.context_length;
    decode_options.attach_kv = options.attach_kv;
    decode_options.kv_shape = options.kv_shape;
    decode_options.layer_count = options.layer_count_seen ? options.layer_count : 0ull;
    decode_options.layer_hidden_dim = options.layer_hidden_dim;
    decode_options.layer_head_dim = options.layer_head_dim;
    decode_options.layer_ffn_dim = options.layer_ffn_dim;
    rc = yvex_engine_decode_step(engine, &decode_options, &decode_summary, &err);
    if (rc != YVEX_OK) {
        const char *status = "decode-step-fail";
        if (decode_summary.decode_phase &&
            strcmp(decode_summary.decode_phase, "after-prefill") == 0 &&
            decode_summary.cleanup_attempted &&
            decode_summary.cleanup_status &&
            strcmp(decode_summary.cleanup_status, "pass") == 0) {
            status = "decode-step-failed-cleaned";
        }
        decode_print_summary(&decode_summary, options.model, options.backend, "pass", status);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    decode_print_summary(&decode_summary, options.model, options.backend,
                         "pass", "decode-step-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

/* Purpose: Render decode help from typed facts (`yvex_decode_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_decode_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex decode --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens "
            "IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--"
            "position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --"
            "kv-capacity N]\n\nDecode creates one bounded diagnostic decode-state step from implemented "
            "prefill/KV state. It does not produce logits, sample, generate, or claim full DeepSeek decode.\n");
}
