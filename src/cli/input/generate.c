/* Owner: src/cli/input
 * Owns: CLI grammar and option validation for the generate command.
 * Does not own: generation execution, report rendering, command dispatch, runtime support, eval, benchmark, or
 *   release decisions.
 * Invariants: parser output is typed; invalid values fail before entering the generation domain.
 * Boundary: parsing arguments is not diagnostic generation execution or support.
 * Purpose: provide cLI grammar and option validation for the generate command.
 * Inputs: bounded command arguments and caller-owned typed request storage.
 * Effects: publishes request fields only after complete grammar validation.
 * Failure: invalid or ambiguous grammar leaves the request uncommitted. */
#include "src/cli/input/private.h"

#include <limits.h>
#include <string.h>

#include <yvex/runtime.h>

/* Purpose: Compute generate arg error for its CLI invariant (`generate_arg_error`). */
static void generate_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "generate", message);
}

/* Purpose: Compute generate arg errorf for its CLI invariant (`generate_arg_errorf`). */
static void generate_arg_errorf(yvex_error *err,
                                const char *fmt,
                                const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "generate", fmt,
                    value ? value : "");
}

/* Purpose: Parse generate parse trace level into typed CLI state (`generate_parse_trace_level`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_parse_trace_level(const char *text,
                                      yvex_generation_trace_level *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "none") == 0) {
        *out = YVEX_GENERATION_TRACE_NONE;
    } else if (strcmp(text, "tokens") == 0) {
        *out = YVEX_GENERATION_TRACE_TOKENS;
    } else if (strcmp(text, "steps") == 0) {
        *out = YVEX_GENERATION_TRACE_STEPS;
    } else if (strcmp(text, "kv") == 0) {
        *out = YVEX_GENERATION_TRACE_KV;
    } else if (strcmp(text, "logits") == 0) {
        *out = YVEX_GENERATION_TRACE_LOGITS;
    } else if (strcmp(text, "sampling") == 0) {
        *out = YVEX_GENERATION_TRACE_SAMPLING;
    } else if (strcmp(text, "full") == 0) {
        *out = YVEX_GENERATION_TRACE_FULL;
    } else {
        return 0;
    }
    return 1;
}

/* Purpose: Parse generate parse output mode into typed CLI state (`generate_parse_output_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_parse_output_mode(const char *text,
                                      yvex_generate_render_mode *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "normal") == 0) {
        *out = YVEX_GENERATE_RENDER_NORMAL;
        return 1;
    }
    if (strcmp(text, "audit") == 0) {
        *out = YVEX_GENERATE_RENDER_AUDIT;
        return 1;
    }
    return 0;
}

/* Purpose: Parse generate parse strategy into typed CLI state (`generate_parse_strategy`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_parse_strategy(const char *text)
{
    return text && strcmp(text, "greedy") == 0;
}

/* Purpose: Compute generate logits count valid for its CLI invariant (`generate_logits_count_valid`). */
static int generate_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

typedef struct {
    int max_new_tokens_seen;
    int kv_shape_seen;
    int layer_hidden_seen;
    int layer_head_seen;
    int layer_ffn_seen;
} generate_parse_state;

/* Parse one shape, position, or KV option; return zero when not handled. */
/* Purpose: Parse generate parse shape option into typed CLI state (`generate_parse_shape_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_parse_shape_option(int argc, char **argv, int *index,
                                       yvex_generation_request *request,
                                       generate_parse_state *state,
                                       yvex_error *err)
{
    const char *arg = argv[*index];
    const char *value = *index + 1 < argc ? argv[*index + 1] : NULL;
    unsigned long long *number = NULL;
    int *seen = NULL;

    if (strcmp(arg, "--attach-kv") == 0) {
        request->attach_kv = 1;
        return 1;
    }
    if (strcmp(arg, "--kv-layers") == 0) {
        number = &request->kv_shape.layer_count;
        seen = &state->kv_shape_seen;
    } else if (strcmp(arg, "--kv-heads") == 0) {
        number = &request->kv_shape.kv_head_count;
        seen = &state->kv_shape_seen;
    } else if (strcmp(arg, "--kv-head-dim") == 0) {
        number = &request->kv_shape.head_dim;
        seen = &state->kv_shape_seen;
    } else if (strcmp(arg, "--kv-capacity") == 0) {
        number = &request->kv_shape.capacity;
        seen = &state->kv_shape_seen;
    } else if (strcmp(arg, "--layers") == 0) {
        number = &request->layer_count;
        seen = &request->layer_count_seen;
    } else if (strcmp(arg, "--layer-hidden-dim") == 0) {
        number = &request->layer_hidden_dim;
        seen = &state->layer_hidden_seen;
    } else if (strcmp(arg, "--layer-head-dim") == 0) {
        number = &request->layer_head_dim;
        seen = &state->layer_head_seen;
    } else if (strcmp(arg, "--layer-ffn-dim") == 0) {
        number = &request->layer_ffn_dim;
        seen = &state->layer_ffn_seen;
    } else if (strcmp(arg, "--chunk-size") == 0) {
        number = &request->chunk_size;
        seen = &request->chunk_size_seen;
    } else if (strcmp(arg, "--context-length") == 0) {
        number = &request->context_length;
        seen = &request->context_length_seen;
    } else if (strcmp(arg, "--position-start") == 0) {
        if (!value || !parse_ull_allow_zero(value,
                                                          &request->position_start)) {
            generate_arg_error(err,
                               "error: --position-start requires a non-negative integer");
            return -1;
        }
        *index += 1;
        return 1;
    } else {
        return 0;
    }
    if (!value || !parse_positive_ull(value, number)) {
        char message[128];
        snprintf(message, sizeof(message), "error: %s requires a positive integer", arg);
        generate_arg_error(err, message);
        return -1;
    }
    if (seen) *seen = 1;
    *index += 1;
    return 1;
}

/* Validate cross-option constraints and install deterministic layer defaults. */
/* Purpose: Validate generate options validate before downstream use (`generate_options_validate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_options_validate(yvex_generation_request *request,
                                     const generate_parse_state *state,
                                     yvex_error *err)
{
    static const char *usage =
        "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --"
            "tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.";
    char message[512];

    if (!request->model_arg || !request->backend_name || !request->segment_name ||
        !request->tokens_text || !state->max_new_tokens_seen) {
        const char *missing = !request->model_arg ? "--model FILE_OR_ALIAS" :
                              !request->backend_name ? "--backend cpu|cuda" :
                              !request->segment_name ? "--segment embedding-rmsnorm" :
                              !request->tokens_text ? "--tokens IDS" : "--max-new-tokens N";
        snprintf(message, sizeof(message), "error: generate requires %s\n%s", missing, usage);
        generate_arg_error(err, message);
        return YVEX_ERR_INVALID_ARG;
    }
    if (!cli_backend_name_valid(request->backend_name)) {
        generate_arg_error(err, "error: --backend supports cpu|cuda for bounded diagnostics");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(request->segment_name, "embedding-rmsnorm") != 0) {
        generate_arg_error(err, "error: generate segment currently supports embedding-rmsnorm for bounded diagnostics");
        return YVEX_ERR_INVALID_ARG;
    }
    if (state->kv_shape_seen && !request->attach_kv) {
        generate_arg_error(err, "error: --kv-* options require --attach-kv");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->attach_kv &&
        (!request->kv_shape.layer_count || !request->kv_shape.kv_head_count ||
         !request->kv_shape.head_dim || !request->kv_shape.capacity)) {
        generate_arg_error(err,
            "error: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->layer_count_seen &&
        (state->layer_hidden_seen || state->layer_head_seen || state->layer_ffn_seen)) {
        generate_arg_error(err, "error: --layer-* options require --layers N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen &&
        (state->layer_hidden_seen || state->layer_head_seen || state->layer_ffn_seen) &&
        !(state->layer_hidden_seen && state->layer_head_seen && state->layer_ffn_seen)) {
        generate_arg_error(err,
            "error: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen && request->layer_count > 16ull) {
        generate_arg_error(err, "error: --layers requires 1 <= N <= 16");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen && !state->layer_hidden_seen) {
        request->layer_hidden_dim = 8ull;
        request->layer_head_dim = 8ull;
        request->layer_ffn_dim = 16ull;
    }
    if (request->layer_count_seen &&
        request->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "generate",
                        "error: --layer-hidden-dim cannot exceed %u for sampled segment handoff",
                        (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: parse generate command CLI arguments into a typed generation request.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_args_parse(int argc,
                             char **argv,
                             yvex_generate_args *out,
                             yvex_error *err)
{
    yvex_generation_request *request;
    generate_parse_state state;
    int i;

    if (!out) {
        generate_arg_error(err, "error: internal parser output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    memset(&state, 0, sizeof(state));
    out->render_mode = YVEX_GENERATE_RENDER_NORMAL;
    request = &out->request;
    request->logits_count = 16ull;
    request->trace_level = YVEX_GENERATION_TRACE_NONE;

    if (argc >= 3 &&
        (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        out->help_requested = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (argc < 3) {
        generate_arg_error(
            err,
            "error: generate requires --model FILE_OR_ALIAS\nusage: yvex generate --model FILE_OR_ALIAS --"
                "backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry "
                "'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --model requires FILE_OR_ALIAS");
                return YVEX_ERR_INVALID_ARG;
            }
            request->model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --backend requires cpu|cuda");
                return YVEX_ERR_INVALID_ARG;
            }
            request->backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --segment requires embedding-rmsnorm");
                return YVEX_ERR_INVALID_ARG;
            }
            request->segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --tokens requires IDS");
                return YVEX_ERR_INVALID_ARG;
            }
            request->tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--max-new-tokens") == 0) {
            if (i + 1 >= argc ||
                !parse_positive_ull(argv[i + 1],
                                                 &request->max_new_tokens)) {
                generate_arg_error(err, "error: --max-new-tokens must be an integer greater than 0");
                return YVEX_ERR_INVALID_ARG;
            }
            state.max_new_tokens_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc || !generate_parse_strategy(argv[i + 1])) {
                generate_arg_error(err, "error: --strategy currently supports greedy only");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--trace-level") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_trace_level(argv[i + 1],
                                            &request->trace_level)) {
                generate_arg_error(err, "error: --trace-level requires none|tokens|steps|kv|logits|sampling|full");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            out->render_mode = YVEX_GENERATE_RENDER_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --output requires normal|audit");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!generate_parse_output_mode(argv[++i], &out->render_mode)) {
                generate_arg_errorf(err, "error: unsupported output mode: %s", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            generate_arg_error(err, "error: JSON output is unsupported for generate; use --output normal|audit");
            return YVEX_ERR_INVALID_ARG;
        } else if (strcmp(argv[i], "--cancel-after-steps") == 0) {
            if (i + 1 >= argc ||
                !parse_ull_allow_zero(argv[i + 1],
                                                   &request->cancel_after_steps)) {
                generate_arg_error(err, "error: --cancel-after-steps must be a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->cancel_after_steps_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc ||
                !parse_positive_ull(argv[i + 1],
                                                 &request->logits_count) ||
                !generate_logits_count_valid(request->logits_count)) {
                generate_arg_error(err, "error: --logits-count requires 1 <= N <= 256");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else {
            int shape_rc = generate_parse_shape_option(argc, argv, &i, request,
                                                       &state, err);
            if (shape_rc < 0) return YVEX_ERR_INVALID_ARG;
            if (shape_rc == 0) {
                generate_arg_errorf(err,
                    "error: unknown generate option: %s\nTry 'yvex help generate' for usage.", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        }
    }

    return generate_options_validate(request, &state, err);
}
