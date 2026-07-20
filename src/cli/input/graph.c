/* Owner: src/cli/input
 * Owns: CLI grammar, option defaults, and parser validation for `yvex graph`.
 * Does not own: model reference resolution, artifact inspection, graph construction, backend probing, primitive
 *   execution, report building, rendering, stdout/stderr output, generation, eval, benchmark, or
 *   release decisions.
 * Invariants: this parser only reads borrowed argc/argv and fills a typed request.
 * Boundary: parsing graph options is not graph runtime support.
 * Purpose: provide cLI grammar, option defaults, and parser validation for `yvex graph`.
 * Inputs: bounded command arguments and caller-owned typed request storage.
 * Effects: publishes request fields only after complete grammar validation.
 * Failure: invalid or ambiguous grammar leaves the request uncommitted. */
#include "src/cli/input/private.h"

#include <string.h>

/* Purpose: Compute graph arg error for its CLI invariant (`graph_arg_error`). */
static int graph_arg_error(yvex_error *err, const char *message) {
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph", message);
    return YVEX_ERR_INVALID_ARG;
}

/* Purpose: Compute graph arg errorf for its CLI invariant (`graph_arg_errorf`). */
static int graph_arg_errorf(yvex_error *err, const char *fmt, const char *value) {
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "graph", fmt, value ? value : "");
    return YVEX_ERR_INVALID_ARG;
}

/* Format a graph refusal that names both the selected mode and conflicting options. */
/* Purpose: Compute graph arg error2 for its CLI invariant (`graph_arg_error2`). */
static int graph_arg_error2(yvex_error *err, const char *fmt, const char *first,
                            const char *second) {
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "graph", fmt, first ? first : "",
                    second ? second : "");
    return YVEX_ERR_INVALID_ARG;
}

/* Purpose: Compute graph mode count for its CLI invariant (`graph_mode_count`). */
static int graph_mode_count(const yvex_graph_report_request *req) {
    return (req->execute_fixture ? 1 : 0) + (req->execute_partial ? 1 : 0) +
           (req->execute_segment ? 1 : 0) + (req->execute_op ? 1 : 0) +
           (req->execute_block ? 1 : 0) + (req->execute_layers ? 1 : 0);
}

/* Purpose: Compute graph args defaults for its CLI invariant (`graph_args_defaults`). */
static void graph_args_defaults(yvex_graph_args *out) {
    memset(out, 0, sizeof(*out));
    out->request.kind = YVEX_GRAPH_REPORT_KIND_GRAPH;
    out->request.action = YVEX_GRAPH_ACTION_DUMP;
    out->request.mode = YVEX_GRAPH_REPORT_MODE_NORMAL;
    out->request.backend = "cpu";
    out->request.sequence_length = 1ull;
    out->request.suite = "all";
    out->request.layers = 2ull;
    out->render_mode = YVEX_GRAPH_REPORT_MODE_NORMAL;
}

typedef enum {
    GRAPH_OPTION_TEXT,
    GRAPH_OPTION_POSITIVE,
    GRAPH_OPTION_NONNEGATIVE,
    GRAPH_OPTION_UINT,
    GRAPH_OPTION_FLAG,
    GRAPH_OPTION_OUTPUT,
    GRAPH_OPTION_AUDIT,
    GRAPH_OPTION_JSON
} graph_option_kind;

enum { GRAPH_PARSE_MAIN = 1u, GRAPH_PARSE_CHECK = 2u, GRAPH_PARSE_ATTENTION = 4u };

enum {
    GRAPH_SEEN_BLOCK = 1u << 0,
    GRAPH_SEEN_TOKEN_INDEX = 1u << 1,
    GRAPH_SEEN_ROPE_POSITION = 1u << 2,
    GRAPH_SEEN_ROPE_HEAD_DIM = 1u << 3,
    GRAPH_SEEN_ATTENTION_SEQ = 1u << 4,
    GRAPH_SEEN_MATMUL_M = 1u << 5,
    GRAPH_SEEN_MATMUL_K = 1u << 6,
    GRAPH_SEEN_MATMUL_N = 1u << 7,
    GRAPH_SEEN_MLP_HIDDEN = 1u << 8,
    GRAPH_SEEN_MLP_FFN = 1u << 9,
    GRAPH_SEEN_MLP_ACTIVATION = 1u << 10,
    GRAPH_SEEN_MLP_EXPERTS = 1u << 11,
    GRAPH_SEEN_MLP_EXPERT_ID = 1u << 12,
    GRAPH_SEEN_LAYERS = 1u << 13
};

typedef struct {
    unsigned int seen;
} graph_parse_state;

typedef struct {
    const char *flag;
    graph_option_kind kind;
    size_t offset;
    size_t companion_offset;
    unsigned int seen;
    unsigned int modes;
    unsigned long long maximum;
    const char *error;
} graph_option_spec;

#define GRAPH_ARG(member) offsetof(yvex_graph_args, member)
#define GRAPH_REQ(member) GRAPH_ARG(request.member)
#define GRAPH_ATTN(member) GRAPH_ARG(attention.member)
#define NO_FIELD ((size_t) - 1)

static const graph_option_spec graph_options[] = {
    {"--model", GRAPH_OPTION_TEXT, GRAPH_REQ(model), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --model requires FILE_OR_ALIAS"},
    {"--seq", GRAPH_OPTION_POSITIVE, GRAPH_REQ(sequence_length), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --seq requires a positive integer"},
    {"--ctx", GRAPH_OPTION_POSITIVE, GRAPH_REQ(context_length), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --ctx requires a positive integer"},
    {"--backend", GRAPH_OPTION_TEXT, GRAPH_REQ(backend), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --backend requires cpu|cuda"},
    {"--execute-fixture", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_fixture), NO_FIELD, 0,
     GRAPH_PARSE_MAIN, 0, NULL},
    {"--fixture-token", GRAPH_OPTION_UINT, GRAPH_REQ(fixture_token), GRAPH_REQ(fixture_token_seen),
     0, GRAPH_PARSE_MAIN, 0, "yvex: --fixture-token requires a non-negative integer"},
    {"--execute-partial", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_partial), NO_FIELD, 0,
     GRAPH_PARSE_MAIN, 0, NULL},
    {"--execute-segment", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_segment), NO_FIELD, 0,
     GRAPH_PARSE_MAIN, 0, NULL},
    {"--execute-op", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_op), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     NULL},
    {"--execute-block", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_block), NO_FIELD, 0, GRAPH_PARSE_MAIN,
     0, NULL},
    {"--execute-layers", GRAPH_OPTION_FLAG, GRAPH_REQ(execute_layers), NO_FIELD, 0,
     GRAPH_PARSE_MAIN, 0, NULL},
    {"--op", GRAPH_OPTION_TEXT, GRAPH_REQ(op), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --op requires rope, attention, matmul, or mlp"},
    {"--block", GRAPH_OPTION_TEXT, GRAPH_REQ(block), NO_FIELD, GRAPH_SEEN_BLOCK, GRAPH_PARSE_MAIN,
     0, "yvex: --block requires fixture"},
    {"--m", GRAPH_OPTION_POSITIVE, GRAPH_REQ(m), NO_FIELD, GRAPH_SEEN_MATMUL_M, GRAPH_PARSE_MAIN, 0,
     "yvex: --m requires a positive integer"},
    {"--k", GRAPH_OPTION_POSITIVE, GRAPH_REQ(k), NO_FIELD, GRAPH_SEEN_MATMUL_K, GRAPH_PARSE_MAIN, 0,
     "yvex: --k requires a positive integer"},
    {"--n", GRAPH_OPTION_POSITIVE, GRAPH_REQ(n), NO_FIELD, GRAPH_SEEN_MATMUL_N, GRAPH_PARSE_MAIN, 0,
     "yvex: --n requires a positive integer"},
    {"--hidden-dim", GRAPH_OPTION_POSITIVE, GRAPH_REQ(hidden_dim), NO_FIELD, GRAPH_SEEN_MLP_HIDDEN,
     GRAPH_PARSE_MAIN, 0, "yvex: --hidden-dim requires a positive integer"},
    {"--ffn-dim", GRAPH_OPTION_POSITIVE, GRAPH_REQ(ffn_dim), NO_FIELD, GRAPH_SEEN_MLP_FFN,
     GRAPH_PARSE_MAIN, 0, "yvex: --ffn-dim requires a positive integer"},
    {"--activation", GRAPH_OPTION_TEXT, GRAPH_REQ(activation), NO_FIELD, GRAPH_SEEN_MLP_ACTIVATION,
     GRAPH_PARSE_MAIN, 0, "yvex: --activation requires silu"},
    {"--gated", GRAPH_OPTION_FLAG, GRAPH_REQ(gated), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0, NULL},
    {"--experts", GRAPH_OPTION_POSITIVE, GRAPH_REQ(experts), NO_FIELD, GRAPH_SEEN_MLP_EXPERTS,
     GRAPH_PARSE_MAIN, 0, "yvex: --experts requires a positive integer"},
    {"--expert-id", GRAPH_OPTION_NONNEGATIVE, GRAPH_REQ(expert_id), GRAPH_REQ(use_expert),
     GRAPH_SEEN_MLP_EXPERT_ID, GRAPH_PARSE_MAIN, 0,
     "yvex: --expert-id requires a non-negative integer"},
    {"--layers", GRAPH_OPTION_POSITIVE, GRAPH_REQ(layers), NO_FIELD, GRAPH_SEEN_LAYERS,
     GRAPH_PARSE_MAIN, 16, "yvex: --layers requires a positive integer"},
    {"--seq-len", GRAPH_OPTION_POSITIVE, GRAPH_REQ(seq_len), NO_FIELD, GRAPH_SEEN_ATTENTION_SEQ,
     GRAPH_PARSE_MAIN, 0, "yvex: --seq-len requires a positive integer"},
    {"--position", GRAPH_OPTION_NONNEGATIVE, GRAPH_REQ(position), NO_FIELD,
     GRAPH_SEEN_ROPE_POSITION, GRAPH_PARSE_MAIN, 0,
     "yvex: --position requires a non-negative integer"},
    {"--head-dim", GRAPH_OPTION_POSITIVE, GRAPH_REQ(head_dim), NO_FIELD, GRAPH_SEEN_ROPE_HEAD_DIM,
     GRAPH_PARSE_MAIN, 0, "yvex: --head-dim requires a positive integer"},
    {"--causal", GRAPH_OPTION_FLAG, GRAPH_REQ(causal), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0, NULL},
    {"--segment", GRAPH_OPTION_TEXT, GRAPH_REQ(segment), NO_FIELD, 0, GRAPH_PARSE_MAIN, 0,
     "yvex: --segment requires embedding-rmsnorm"},
    {"--partial-token", GRAPH_OPTION_UINT, GRAPH_REQ(partial_token), GRAPH_REQ(partial_token_seen),
     0, GRAPH_PARSE_MAIN, 0, "yvex: --partial-token requires a non-negative integer"},
    {"--tokens", GRAPH_OPTION_TEXT, GRAPH_REQ(tokens_text), GRAPH_REQ(tokens_seen), 0,
     GRAPH_PARSE_MAIN, 0, "yvex: --tokens requires comma-separated token IDs"},
    {"--token-index", GRAPH_OPTION_NONNEGATIVE, GRAPH_REQ(token_index), GRAPH_REQ(token_index_seen),
     GRAPH_SEEN_TOKEN_INDEX, GRAPH_PARSE_MAIN, 0,
     "yvex: --token-index requires a non-negative integer"},
    {"--backend", GRAPH_OPTION_TEXT, GRAPH_REQ(backend), NO_FIELD, 0, GRAPH_PARSE_CHECK, 0,
     "yvex: graph check --backend requires cpu|cuda"},
    {"--suite", GRAPH_OPTION_TEXT, GRAPH_REQ(suite), NO_FIELD, 0, GRAPH_PARSE_CHECK, 0,
     "yvex: graph check --suite requires primitives, block, layers, or all"},
    {"--layers", GRAPH_OPTION_POSITIVE, GRAPH_REQ(layers), NO_FIELD, 0, GRAPH_PARSE_CHECK, 16,
     "yvex: graph check --layers requires a positive integer"},
    {"--target", GRAPH_OPTION_TEXT, GRAPH_ATTN(target), NO_FIELD, 0, GRAPH_PARSE_ATTENTION, 0,
     "yvex: graph attention option requires a value"},
    {"--artifact", GRAPH_OPTION_TEXT, GRAPH_ATTN(artifact_path), NO_FIELD, 0, GRAPH_PARSE_ATTENTION,
     0, "yvex: graph attention option requires a value"},
    {"--models-root", GRAPH_OPTION_TEXT, GRAPH_ATTN(models_root), NO_FIELD, 0,
     GRAPH_PARSE_ATTENTION, 0, "yvex: graph attention option requires a value"},
    {"--backend", GRAPH_OPTION_TEXT, GRAPH_ATTN(backend), NO_FIELD, 0, GRAPH_PARSE_ATTENTION, 0,
     "yvex: graph attention option requires a value"},
    {"--probe", GRAPH_OPTION_TEXT, GRAPH_ATTN(probe), NO_FIELD, 0, GRAPH_PARSE_ATTENTION, 0,
     "yvex: graph attention option requires a value"},
    {"--scope", GRAPH_OPTION_TEXT, GRAPH_ATTN(scope), NO_FIELD, 0, GRAPH_PARSE_ATTENTION, 0,
     "yvex: graph attention option requires a value"},
    {"--compare-backends", GRAPH_OPTION_FLAG, GRAPH_ATTN(compare_backends), NO_FIELD, 0,
     GRAPH_PARSE_ATTENTION, 0, NULL},
    {"--output", GRAPH_OPTION_OUTPUT, NO_FIELD, NO_FIELD, 0,
     GRAPH_PARSE_MAIN | GRAPH_PARSE_CHECK | GRAPH_PARSE_ATTENTION, 0,
     "yvex: --output requires normal|table|audit"},
    {"--audit", GRAPH_OPTION_AUDIT, NO_FIELD, NO_FIELD, 0,
     GRAPH_PARSE_MAIN | GRAPH_PARSE_CHECK | GRAPH_PARSE_ATTENTION, 0, NULL},
    {"--json", GRAPH_OPTION_JSON, NO_FIELD, NO_FIELD, 0, GRAPH_PARSE_ATTENTION, 0, NULL},
};

#undef GRAPH_ATTN
#undef GRAPH_REQ
#undef GRAPH_ARG

/* Purpose: resolve one option in the immutable grammar for the selected command mode. */
static const graph_option_spec *graph_option_find(const char *flag, unsigned int mode) {
    size_t index;

    for (index = 0; index < sizeof(graph_options) / sizeof(graph_options[0]); ++index) {
        if ((graph_options[index].modes & mode) && strcmp(flag, graph_options[index].flag) == 0)
            return &graph_options[index];
    }
    return NULL;
}

/* Purpose: Parse an output mode.
 * Inputs: text and output.
 * Effects: sets the enum.
 * Failure: returns false.
 * Boundary: CLI grammar. */
static int graph_parse_output_mode(const char *text, int allow_json, yvex_graph_report_mode *mode) {
    static const char *const names[] = {"normal", "table", "audit", "json"};
    yvex_graph_report_mode modes[] = {YVEX_GRAPH_REPORT_MODE_NORMAL, YVEX_GRAPH_REPORT_MODE_TABLE,
                                      YVEX_GRAPH_REPORT_MODE_AUDIT, YVEX_GRAPH_REPORT_MODE_JSON};
    size_t count = allow_json ? 4u : 3u;
    size_t index;

    if (!text || !mode)
        return 0;
    for (index = 0; index < count; ++index) {
        if (strcmp(text, names[index]) == 0) {
            *mode = modes[index];
            return 1;
        }
    }
    return 0;
}

/* Purpose: Bind one option.
 * Inputs: schema, argv, and cursor.
 * Effects: updates request and cursor.
 * Failure: typed refusal.
 * Boundary: no artifact or runtime access. */
static int graph_option_bind(const graph_option_spec *spec, unsigned int mode, int argc,
                             char **argv, int *index, yvex_graph_args *out,
                             graph_parse_state *state, yvex_error *err) {
    unsigned char *field = spec->offset == NO_FIELD ? NULL : (unsigned char *)out + spec->offset;
    const char *value;

    if (spec->kind == GRAPH_OPTION_FLAG)
        *(int *)field = 1;
    else if (spec->kind == GRAPH_OPTION_AUDIT || spec->kind == GRAPH_OPTION_JSON) {
        out->render_mode = spec->kind == GRAPH_OPTION_JSON ? YVEX_GRAPH_REPORT_MODE_JSON
                                                           : YVEX_GRAPH_REPORT_MODE_AUDIT;
    } else {
        if (*index + 1 >= argc) {
            graph_arg_error(err, spec->error);
            return 0;
        }
        value = argv[++*index];
        if (spec->kind == GRAPH_OPTION_TEXT)
            *(const char **)field = value;
        else if (spec->kind == GRAPH_OPTION_OUTPUT) {
            if (!graph_parse_output_mode(value, mode == GRAPH_PARSE_ATTENTION, &out->render_mode)) {
                graph_arg_errorf(err, "yvex: unsupported graph output mode: %s", value);
                return 0;
            }
        } else if (spec->kind == GRAPH_OPTION_UINT) {
            if (!parse_uint_allow_zero(value, (unsigned int *)field)) {
                graph_arg_error(err, spec->error);
                return 0;
            }
        } else if (!(spec->kind == GRAPH_OPTION_POSITIVE
                         ? parse_positive_ull(value, (unsigned long long *)field)
                         : parse_ull_allow_zero(value, (unsigned long long *)field))) {
            graph_arg_error(err, spec->error);
            return 0;
        }
        if (spec->maximum && *(unsigned long long *)field > spec->maximum) {
            graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
            return 0;
        }
    }
    if (spec->companion_offset != NO_FIELD)
        *(int *)((unsigned char *)out + spec->companion_offset) = 1;
    state->seen |= spec->seen;
    out->request.mode = out->render_mode;
    return 1;
}

/* Purpose: Parse attention execution.
 * Inputs: argv and output.
 * Effects: publishes options.
 * Failure: typed refusal.
 * Boundary: no artifact or graph execution. */
static int graph_parse_attention_execute(int argc, char **argv, yvex_graph_args *out,
                                         yvex_error *err) {
    graph_parse_state state = {0};
    int i;

    out->attention.execute = 1;
    out->attention.probe = "canonical";
    out->attention.scope = "quick";
    for (i = 4; i < argc; ++i) {
        const graph_option_spec *spec;

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->help_requested = 1;
            return YVEX_OK;
        }
        spec = graph_option_find(argv[i], GRAPH_PARSE_ATTENTION);
        if (!spec) {
            graph_arg_errorf(err, "yvex: unknown graph attention option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
        if (!graph_option_bind(spec, GRAPH_PARSE_ATTENTION, argc, argv, &i, out, &state, err))
            return YVEX_ERR_INVALID_ARG;
    }
    if (!out->attention.target) {
        graph_arg_error(err, "yvex: graph attention execute requires --target TARGET");
        return YVEX_ERR_INVALID_ARG;
    }
    if (out->attention.compare_backends == (out->attention.backend != NULL)) {
        graph_arg_error(err, out->attention.compare_backends
                                 ? "yvex: --compare-backends cannot be combined with --backend"
                                 : "yvex: graph attention execute requires --backend cpu|cuda or "
                                   "--compare-backends");
        return YVEX_ERR_INVALID_ARG;
    }
    if (out->attention.backend && !cli_backend_name_valid(out->attention.backend)) {
        graph_arg_errorf(err, "yvex: unknown backend kind: %s", out->attention.backend);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(out->attention.probe, "canonical") != 0) {
        graph_arg_errorf(err, "yvex: unsupported attention probe: %s", out->attention.probe);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(out->attention.scope, "quick") != 0 && strcmp(out->attention.scope, "full") != 0) {
        graph_arg_errorf(err, "yvex: unsupported attention scope: %s", out->attention.scope);
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Validate check syntax.
 * Inputs: request.
 * Effects: none.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_validate_check(yvex_graph_report_request *req, yvex_error *err) {
    if (!cli_backend_name_valid(req->backend))
        return graph_arg_errorf(err, "yvex: unknown backend kind: %s", req->backend);
    if (strcmp(req->suite, "primitives") != 0 && strcmp(req->suite, "block") != 0 &&
        strcmp(req->suite, "layers") != 0 && strcmp(req->suite, "all") != 0)
        return graph_arg_error(
            err, "yvex: graph check --suite requires primitives, block, layers, or all");
    if (req->layers == 0ull || req->layers > 16ull)
        return graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
    return YVEX_OK;
}

/* Purpose: Validate block or layer syntax.
 * Inputs: request and seen state.
 * Effects: none.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_validate_block_mode(yvex_graph_report_request *req, const graph_parse_state *state,
                                     int layers_mode, yvex_error *err) {
    const unsigned int required = GRAPH_SEEN_ATTENTION_SEQ | GRAPH_SEEN_ROPE_POSITION |
                                  GRAPH_SEEN_MLP_HIDDEN | GRAPH_SEEN_ROPE_HEAD_DIM |
                                  GRAPH_SEEN_MLP_FFN;
    const unsigned int standalone = GRAPH_SEEN_MATMUL_M | GRAPH_SEEN_MATMUL_K |
                                    GRAPH_SEEN_MATMUL_N | GRAPH_SEEN_MLP_ACTIVATION |
                                    GRAPH_SEEN_MLP_EXPERTS | GRAPH_SEEN_MLP_EXPERT_ID |
                                    GRAPH_SEEN_TOKEN_INDEX;
    const char *mode = layers_mode ? "--execute-layers" : "--execute-block";

    if (req->model)
        return graph_arg_errorf(err, "yvex: %s does not take a model artifact", mode);
    if (!(state->seen & GRAPH_SEEN_BLOCK))
        return graph_arg_errorf(err, "yvex: %s requires --block fixture", mode);
    if (strcmp(req->block, "fixture") != 0)
        return graph_arg_errorf(err, "yvex: unsupported block: %s", req->block);
    if (layers_mode && !(state->seen & GRAPH_SEEN_LAYERS))
        return graph_arg_error(err, "yvex: --execute-layers requires --layers N");
    if ((state->seen & required) != required)
        return graph_arg_errorf(
            err,
            "yvex: %s requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N",
            mode);
    if (req->position >= req->seq_len)
        return graph_arg_error(err, "yvex: position must be less than seq-len");
    if (req->hidden_dim % req->head_dim != 0ull)
        return graph_arg_error(err, "yvex: hidden-dim must be divisible by head-dim");
    if (req->op || (state->seen & standalone) || req->fixture_token_seen ||
        req->partial_token_seen || req->tokens_seen || req->segment)
        return graph_arg_error2(err, "yvex: %s cannot be combined with %s", mode,
                                layers_mode
                                    ? "model graph, segment, token, or standalone op options"
                                    : "model graph, segment, or standalone op options");
    return YVEX_OK;
}

/* Purpose: Validate primitive syntax.
 * Inputs: request and seen state.
 * Effects: none.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_validate_op_mode(yvex_graph_report_request *req, const graph_parse_state *state,
                                  yvex_error *err) {
    const unsigned int matmul = GRAPH_SEEN_MATMUL_M | GRAPH_SEEN_MATMUL_K | GRAPH_SEEN_MATMUL_N;
    const unsigned int position = GRAPH_SEEN_ROPE_POSITION | GRAPH_SEEN_ROPE_HEAD_DIM;
    const unsigned int mlp = GRAPH_SEEN_MLP_HIDDEN | GRAPH_SEEN_MLP_FFN |
                             GRAPH_SEEN_MLP_ACTIVATION | GRAPH_SEEN_MLP_EXPERTS |
                             GRAPH_SEEN_MLP_EXPERT_ID;
    const unsigned int mlp_required =
        GRAPH_SEEN_MLP_HIDDEN | GRAPH_SEEN_MLP_FFN | GRAPH_SEEN_MLP_ACTIVATION;
    unsigned int seen = state->seen;

    if (req->model)
        return graph_arg_error(err, "yvex: --execute-op does not take a model artifact");
    if (!req->op || (strcmp(req->op, "rope") != 0 && strcmp(req->op, "attention") != 0 &&
                     strcmp(req->op, "matmul") != 0 && strcmp(req->op, "mlp") != 0))
        return graph_arg_error(
            err, "yvex: --execute-op requires --op rope, --op attention, --op matmul, or --op mlp");
    if (strcmp(req->op, "matmul") == 0) {
        if ((seen & matmul) != matmul)
            return graph_arg_error(err,
                                   "yvex: --execute-op --op matmul requires --m M --k K --n N");
        if ((seen & (position | GRAPH_SEEN_ATTENTION_SEQ)) || req->causal)
            return graph_arg_error(err, "yvex: --position, --head-dim, --seq-len, and --causal "
                                        "require --op rope or --op attention");
    } else if (strcmp(req->op, "mlp") == 0) {
        if ((seen & mlp_required) != mlp_required || !req->gated)
            return graph_arg_error(err, "yvex: --execute-op --op mlp requires --hidden-dim N "
                                        "--ffn-dim N --activation silu --gated");
        if ((seen & (position | GRAPH_SEEN_ATTENTION_SEQ | matmul)) || req->causal)
            return graph_arg_error(err, "yvex: --op mlp cannot use --position, --head-dim, "
                                        "--seq-len, --causal, --m, --k, or --n");
        if (!!(seen & GRAPH_SEEN_MLP_EXPERTS) != !!(seen & GRAPH_SEEN_MLP_EXPERT_ID))
            return graph_arg_error(err,
                                   "yvex: --experts and --expert-id must be provided together");
    } else if (!(seen & GRAPH_SEEN_ROPE_POSITION))
        return graph_arg_error(err, "yvex: --execute-op requires --position N");
    if (strcmp(req->op, "matmul") != 0 && strcmp(req->op, "mlp") != 0 &&
        !(seen & GRAPH_SEEN_ROPE_HEAD_DIM))
        return graph_arg_error(err, "yvex: --execute-op requires --head-dim N");
    if (strcmp(req->op, "attention") == 0 && !(seen & GRAPH_SEEN_ATTENTION_SEQ))
        return graph_arg_error(err, "yvex: --execute-op --op attention requires --seq-len N");
    if (strcmp(req->op, "rope") == 0 && ((seen & GRAPH_SEEN_ATTENTION_SEQ) || req->causal))
        return graph_arg_error(err, "yvex: --seq-len and --causal require --op attention");
    if (strcmp(req->op, "rope") != 0 && strcmp(req->op, "attention") != 0 && (seen & position))
        return graph_arg_error(
            err, "yvex: --position and --head-dim require --op rope or --op attention");
    if (strcmp(req->op, "matmul") != 0 && (seen & matmul))
        return graph_arg_error(err, "yvex: --m, --k, and --n require --op matmul");
    if (strcmp(req->op, "mlp") != 0 && ((seen & mlp) || req->gated))
        return graph_arg_error(err, "yvex: --hidden-dim, --ffn-dim, --activation, --gated, "
                                    "--experts, and --expert-id require --op mlp");
    if (req->fixture_token_seen || req->partial_token_seen || req->tokens_seen ||
        (seen & GRAPH_SEEN_TOKEN_INDEX) || req->segment)
        return graph_arg_error(
            err, "yvex: --execute-op cannot be combined with model graph token or segment options");
    return YVEX_OK;
}

/* Purpose: Validate model syntax.
 * Inputs: request and seen state.
 * Effects: selects the action.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_validate_model_mode(yvex_graph_report_request *req, const graph_parse_state *state,
                                     yvex_error *err) {
    const unsigned int standalone =
        GRAPH_SEEN_ROPE_POSITION | GRAPH_SEEN_ROPE_HEAD_DIM | GRAPH_SEEN_ATTENTION_SEQ |
        GRAPH_SEEN_MATMUL_M | GRAPH_SEEN_MATMUL_K | GRAPH_SEEN_MATMUL_N | GRAPH_SEEN_MLP_HIDDEN |
        GRAPH_SEEN_MLP_FFN | GRAPH_SEEN_MLP_ACTIVATION | GRAPH_SEEN_MLP_EXPERTS |
        GRAPH_SEEN_MLP_EXPERT_ID | GRAPH_SEEN_LAYERS;

    if (state->seen & GRAPH_SEEN_BLOCK)
        return graph_arg_error(err, "yvex: --block requires --execute-block or --execute-layers");
    if (req->op || (state->seen & standalone) || req->causal || req->gated)
        return graph_arg_error(err, "yvex: --op and standalone op options require --execute-op");
    if (req->execute_segment) {
        if (!req->segment)
            return graph_arg_error(err,
                                   "yvex: --execute-segment requires --segment embedding-rmsnorm");
        if (strcmp(req->segment, "embedding-rmsnorm") != 0)
            return graph_arg_errorf(err, "yvex: unsupported segment: %s", req->segment);
    } else if (req->segment)
        return graph_arg_error(err, "yvex: --segment requires --execute-segment");
    if ((state->seen & GRAPH_SEEN_TOKEN_INDEX) && !req->tokens_seen)
        return graph_arg_error(err, "yvex: --token-index requires --tokens");
    if (req->tokens_seen && !(req->execute_fixture || req->execute_partial || req->execute_segment))
        return graph_arg_error(err, "yvex: --tokens is only supported with graph execution flags");
    if (req->tokens_seen && (req->partial_token_seen || req->fixture_token_seen))
        return graph_arg_error(
            err, "yvex: --tokens cannot be combined with --partial-token or --fixture-token");
    if (req->execute_fixture)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_FIXTURE;
    else if (req->execute_segment)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_SEGMENT;
    else if (req->execute_partial)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_PARTIAL;
    return YVEX_OK;
}

/* Purpose: Validate selected graph mode.
 * Inputs: request and state.
 * Effects: none.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_validate_modes(yvex_graph_report_request *req, const graph_parse_state *state,
                                yvex_error *err) {
    if (!req->model && !req->execute_op && !req->execute_block && !req->execute_layers)
        return graph_arg_error(err, "yvex: graph requires FILE_OR_ALIAS");
    if (!cli_backend_name_valid(req->backend))
        return graph_arg_errorf(err, "yvex: unknown backend kind: %s", req->backend);
    if (graph_mode_count(req) > 1)
        return graph_arg_error(err, "yvex: graph execution flags are mutually exclusive");
    if (req->execute_layers)
        return graph_validate_block_mode(req, state, 1, err);
    if (req->execute_block)
        return graph_validate_block_mode(req, state, 0, err);
    if (req->execute_op)
        return graph_validate_op_mode(req, state, err);
    return graph_validate_model_mode(req, state, err);
}

/* Purpose: Parse graph check.
 * Inputs: argv and output.
 * Effects: builds request.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_parse_check(int argc, char **argv, yvex_graph_args *out, yvex_error *err) {
    yvex_graph_report_request *req = &out->request;
    graph_parse_state state = {0};
    int i;

    req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    req->action = YVEX_GRAPH_ACTION_CHECK;
    req->suite = "all";
    req->layers = 2ull;
    for (i = 3; i < argc; ++i) {
        const graph_option_spec *spec = graph_option_find(argv[i], GRAPH_PARSE_CHECK);

        if (spec) {
            if (!graph_option_bind(spec, GRAPH_PARSE_CHECK, argc, argv, &i, out, &state, err))
                return YVEX_ERR_INVALID_ARG;
        } else if (argv[i][0] == '-' && strcmp(argv[i], "--model") != 0) {
            graph_arg_errorf(err, "yvex: unknown graph check option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        } else {
            graph_arg_error(err, "yvex: graph check preset is fixture-only in GRAPH.CHECK.0");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    return graph_validate_check(req, err);
}

/* Purpose: Parse a legacy option.
 * Inputs: argv and cursor.
 * Effects: updates request.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
static int graph_parse_main_option(int argc, char **argv, int *index, yvex_graph_args *out,
                                   graph_parse_state *state, yvex_error *err) {
    const graph_option_spec *spec = graph_option_find(argv[*index], GRAPH_PARSE_MAIN);

    if (spec)
        return graph_option_bind(spec, GRAPH_PARSE_MAIN, argc, argv, index, out, state, err);
    if (!out->request.model) {
        out->request.model = argv[*index];
        return 1;
    }
    graph_arg_errorf(err, "yvex: unknown graph option: %s", argv[*index]);
    return 0;
}
/* Purpose: Parse graph argv.
 * Inputs: argv and output.
 * Effects: publishes request.
 * Failure: typed refusal.
 * Boundary: CLI grammar. */
int yvex_graph_args_parse(int argc, char **argv, yvex_graph_args *out, yvex_error *err) {
    yvex_graph_report_request *req;
    graph_parse_state state;
    int i;

    if (!out) {
        graph_arg_error(err, "graph parser requires output");
        return YVEX_ERR_INVALID_ARG;
    }
    graph_args_defaults(out);
    memset(&state, 0, sizeof(state));
    req = &out->request;

    if (argc < 3) {
        out->help_requested = 1;
        out->help_exit_code = 2;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        out->help_requested = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (strcmp(argv[2], "check") == 0) {
        return graph_parse_check(argc, argv, out, err);
    }
    if (strcmp(argv[2], "attention") == 0) {
        if (argc == 4 && (strcmp(argv[3], "--help") == 0 || strcmp(argv[3], "-h") == 0)) {
            out->help_requested = 1;
            yvex_error_clear(err);
            return YVEX_OK;
        }
        if (argc < 4 || strcmp(argv[3], "execute") != 0) {
            graph_arg_error(err, "yvex: graph attention requires the execute action");
            return YVEX_ERR_INVALID_ARG;
        }
        return graph_parse_attention_execute(argc, argv, out, err);
    }

    for (i = 2; i < argc; ++i) {
        if (!graph_parse_main_option(argc, argv, &i, out, &state, err))
            return YVEX_ERR_INVALID_ARG;
    }
    if (req->execute_op)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_OP;
    else if (req->execute_block)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_BLOCK;
    else if (req->execute_layers)
        req->action = YVEX_GRAPH_ACTION_EXECUTE_LAYERS;
    if (req->execute_op || req->execute_block || req->execute_layers)
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    return graph_validate_modes(req, &state, err);
}
