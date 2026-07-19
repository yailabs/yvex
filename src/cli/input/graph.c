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
static void graph_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph", message);
}

/* Purpose: Compute graph arg errorf for its CLI invariant (`graph_arg_errorf`). */
static void graph_arg_errorf(yvex_error *err,
                             const char *fmt,
                             const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "graph", fmt,
                    value ? value : "");
}

/* Format a graph refusal that names both the selected mode and conflicting options. */
/* Purpose: Compute graph arg error2 for its CLI invariant (`graph_arg_error2`). */
static void graph_arg_error2(yvex_error *err,
                             const char *fmt,
                             const char *first,
                             const char *second)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "graph", fmt,
                    first ? first : "", second ? second : "");
}

/* Purpose: Compute graph mode count for its CLI invariant (`graph_mode_count`). */
static int graph_mode_count(const yvex_graph_report_request *req)
{
    return (req->execute_fixture ? 1 : 0) +
           (req->execute_partial ? 1 : 0) +
           (req->execute_segment ? 1 : 0) +
           (req->execute_op ? 1 : 0) +
           (req->execute_block ? 1 : 0) +
           (req->execute_layers ? 1 : 0);
}

/* Purpose: Compute graph args defaults for its CLI invariant (`graph_args_defaults`). */
static void graph_args_defaults(yvex_graph_args *out)
{
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

/* Purpose: Compute graph require value for its CLI invariant (`graph_require_value`). */
static int graph_require_value(int argc,
                               int index,
                               const char *message,
                               yvex_error *err)
{
    if (index + 1 >= argc) {
        graph_arg_error(err, message);
        return 0;
    }
    return 1;
}

/* Purpose: Parse graph parse output mode into typed CLI state (`graph_parse_output_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_parse_output_mode(const char *text,
                                   yvex_graph_report_mode *mode)
{
    if (!text || !mode) {
        return 0;
    }
    if (strcmp(text, "normal") == 0) {
        *mode = YVEX_GRAPH_REPORT_MODE_NORMAL;
        return 1;
    }
    if (strcmp(text, "table") == 0) {
        *mode = YVEX_GRAPH_REPORT_MODE_TABLE;
        return 1;
    }
    if (strcmp(text, "audit") == 0) {
        *mode = YVEX_GRAPH_REPORT_MODE_AUDIT;
        return 1;
    }
    return 0;
}

/* Purpose: Validate graph validate check before downstream use (`graph_validate_check`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_validate_check(yvex_graph_report_request *req,
                                yvex_error *err)
{
    if (!cli_backend_name_valid(req->backend)) {
        graph_arg_errorf(err, "yvex: unknown backend kind: %s", req->backend);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->suite, "primitives") != 0 &&
        strcmp(req->suite, "block") != 0 &&
        strcmp(req->suite, "layers") != 0 &&
        strcmp(req->suite, "all") != 0) {
        graph_arg_error(err,
            "yvex: graph check --suite requires primitives, block, layers, or all");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layers == 0ull || req->layers > 16ull) {
        graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

typedef struct {
    int block_name;
    int token_index;
    int rope_position;
    int rope_head_dim;
    int attention_seq_len;
    int matmul_m;
    int matmul_k;
    int matmul_n;
    int mlp_hidden_dim;
    int mlp_ffn_dim;
    int mlp_activation;
    int mlp_experts;
    int mlp_expert_id;
    int layers;
} graph_parse_state;

typedef enum graph_detail_kind {
    GRAPH_DETAIL_POSITIVE,
    GRAPH_DETAIL_NONNEGATIVE,
    GRAPH_DETAIL_UINT,
    GRAPH_DETAIL_TEXT,
    GRAPH_DETAIL_FLAG,
    GRAPH_DETAIL_OUTPUT,
    GRAPH_DETAIL_AUDIT
} graph_detail_kind;

typedef struct graph_detail_spec {
    const char *flag;
    graph_detail_kind kind;
    size_t request_offset;
    size_t state_offset;
    size_t companion_offset;
    unsigned long long maximum;
    const char *error;
} graph_detail_spec;

#define GRAPH_REQ(member_) offsetof(yvex_graph_report_request, member_)
#define GRAPH_STATE(member_) offsetof(graph_parse_state, member_)
#define NO_FIELD ((size_t)-1)

static const graph_detail_spec graph_detail_options[] = {
    {"--m", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(m), GRAPH_STATE(matmul_m), NO_FIELD, 0ull,
     "yvex: --m requires a positive integer"},
    {"--k", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(k), GRAPH_STATE(matmul_k), NO_FIELD, 0ull,
     "yvex: --k requires a positive integer"},
    {"--n", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(n), GRAPH_STATE(matmul_n), NO_FIELD, 0ull,
     "yvex: --n requires a positive integer"},
    {"--hidden-dim", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(hidden_dim), GRAPH_STATE(mlp_hidden_dim),
     NO_FIELD, 0ull, "yvex: --hidden-dim requires a positive integer"},
    {"--ffn-dim", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(ffn_dim), GRAPH_STATE(mlp_ffn_dim),
     NO_FIELD, 0ull, "yvex: --ffn-dim requires a positive integer"},
    {"--activation", GRAPH_DETAIL_TEXT, GRAPH_REQ(activation), GRAPH_STATE(mlp_activation),
     NO_FIELD, 0ull, "yvex: --activation requires silu"},
    {"--gated", GRAPH_DETAIL_FLAG, GRAPH_REQ(gated), NO_FIELD, NO_FIELD, 0ull, NULL},
    {"--experts", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(experts), GRAPH_STATE(mlp_experts),
     NO_FIELD, 0ull, "yvex: --experts requires a positive integer"},
    {"--expert-id", GRAPH_DETAIL_NONNEGATIVE, GRAPH_REQ(expert_id), GRAPH_STATE(mlp_expert_id),
     GRAPH_REQ(use_expert), 0ull, "yvex: --expert-id requires a non-negative integer"},
    {"--layers", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(layers), GRAPH_STATE(layers), NO_FIELD, 16ull,
     "yvex: --layers requires a positive integer"},
    {"--seq-len", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(seq_len), GRAPH_STATE(attention_seq_len),
     NO_FIELD, 0ull, "yvex: --seq-len requires a positive integer"},
    {"--position", GRAPH_DETAIL_NONNEGATIVE, GRAPH_REQ(position), GRAPH_STATE(rope_position),
     NO_FIELD, 0ull, "yvex: --position requires a non-negative integer"},
    {"--head-dim", GRAPH_DETAIL_POSITIVE, GRAPH_REQ(head_dim), GRAPH_STATE(rope_head_dim),
     NO_FIELD, 0ull, "yvex: --head-dim requires a positive integer"},
    {"--causal", GRAPH_DETAIL_FLAG, GRAPH_REQ(causal), NO_FIELD, NO_FIELD, 0ull, NULL},
    {"--segment", GRAPH_DETAIL_TEXT, GRAPH_REQ(segment), NO_FIELD, NO_FIELD, 0ull,
     "yvex: --segment requires embedding-rmsnorm"},
    {"--partial-token", GRAPH_DETAIL_UINT, GRAPH_REQ(partial_token), NO_FIELD,
     GRAPH_REQ(partial_token_seen), 0ull,
     "yvex: --partial-token requires a non-negative integer"},
    {"--tokens", GRAPH_DETAIL_TEXT, GRAPH_REQ(tokens_text), NO_FIELD, GRAPH_REQ(tokens_seen),
     0ull, "yvex: --tokens requires comma-separated token IDs"},
    {"--token-index", GRAPH_DETAIL_NONNEGATIVE, GRAPH_REQ(token_index), GRAPH_STATE(token_index),
     GRAPH_REQ(token_index_seen), 0ull,
     "yvex: --token-index requires a non-negative integer"},
    {"--output", GRAPH_DETAIL_OUTPUT, NO_FIELD, NO_FIELD, NO_FIELD, 0ull,
     "yvex: --output requires normal|table|audit"},
    {"--audit", GRAPH_DETAIL_AUDIT, NO_FIELD, NO_FIELD, NO_FIELD, 0ull, NULL},
};

#undef GRAPH_STATE
#undef GRAPH_REQ

/* Locate one standalone graph option in its immutable grammar table. */
/* Purpose: Compute graph detail find for its CLI invariant (`graph_detail_find`). */
static const graph_detail_spec *graph_detail_find(const char *flag)
{
    size_t i;

    for (i = 0; i < sizeof(graph_detail_options) / sizeof(graph_detail_options[0]); ++i) {
        if (strcmp(flag, graph_detail_options[i].flag) == 0) return &graph_detail_options[i];
    }
    return NULL;
}

/* Bind one graph detail option while preserving typed seen-state facts. */
/* Purpose: Compute graph detail bind for its CLI invariant (`graph_detail_bind`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_detail_bind(const graph_detail_spec *spec,
                             int argc,
                             char **argv,
                             int *index,
                             yvex_graph_args *out,
                             graph_parse_state *state,
                             yvex_error *err)
{
    yvex_graph_report_request *request = &out->request;
    unsigned char *field = spec->request_offset == NO_FIELD
                               ? NULL
                               : (unsigned char *)request + spec->request_offset;
    const char *value;

    if (spec->kind == GRAPH_DETAIL_FLAG) *(int *)field = 1;
    else if (spec->kind == GRAPH_DETAIL_AUDIT) {
        out->render_mode = YVEX_GRAPH_REPORT_MODE_AUDIT;
        request->mode = out->render_mode;
    } else {
        if (*index + 1 >= argc) {
            graph_arg_error(err, spec->error);
            return -1;
        }
        value = argv[++*index];
        if (spec->kind == GRAPH_DETAIL_TEXT) *(const char **)field = value;
        else if (spec->kind == GRAPH_DETAIL_OUTPUT) {
            if (!graph_parse_output_mode(value, &out->render_mode)) {
                graph_arg_errorf(err, "yvex: unsupported graph output mode: %s", value);
                return -1;
            }
            request->mode = out->render_mode;
        } else if (spec->kind == GRAPH_DETAIL_UINT) {
            if (!parse_uint_allow_zero(value, (unsigned int *)field)) {
                graph_arg_error(err, spec->error);
                return -1;
            }
        } else if (!(spec->kind == GRAPH_DETAIL_POSITIVE
                         ? parse_positive_ull(value, (unsigned long long *)field)
                         : parse_ull_allow_zero(value, (unsigned long long *)field))) {
            graph_arg_error(err, spec->error);
            return -1;
        }
        if (spec->maximum && *(unsigned long long *)field > spec->maximum) {
            graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
            return -1;
        }
    }
    if (spec->state_offset != NO_FIELD) {
        *(int *)((unsigned char *)state + spec->state_offset) = 1;
    }
    if (spec->companion_offset != NO_FIELD) {
        *(int *)((unsigned char *)request + spec->companion_offset) = 1;
    }
    return 1;
}

#undef NO_FIELD

/* Validate fixture block/layer execution without model-owned inputs. */
/* Purpose: Validate graph validate block mode before downstream use (`graph_validate_block_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_validate_block_mode(yvex_graph_report_request *req,
                                     const graph_parse_state *state,
                                     int layers_mode,
                                     yvex_error *err)
{
    const char *mode = layers_mode ? "--execute-layers" : "--execute-block";

    if (req->model) {
        graph_arg_errorf(err, "yvex: %s does not take a model artifact", mode);
        return YVEX_ERR_INVALID_ARG;
    }
    if (!state->block_name) {
        graph_arg_errorf(err, "yvex: %s requires --block fixture", mode);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->block, "fixture") != 0) {
        graph_arg_errorf(err, "yvex: unsupported block: %s", req->block);
        return YVEX_ERR_INVALID_ARG;
    }
    if (layers_mode && !state->layers) {
        graph_arg_error(err, "yvex: --execute-layers requires --layers N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!state->attention_seq_len || !state->rope_position ||
        !state->mlp_hidden_dim || !state->rope_head_dim || !state->mlp_ffn_dim) {
        graph_arg_errorf(err,
            "yvex: %s requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N",
            mode);
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->position >= req->seq_len) {
        graph_arg_error(err, "yvex: position must be less than seq-len");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->hidden_dim % req->head_dim != 0ull) {
        graph_arg_error(err, "yvex: hidden-dim must be divisible by head-dim");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->op || state->matmul_m || state->matmul_k || state->matmul_n ||
        state->mlp_activation || state->mlp_experts || state->mlp_expert_id ||
        req->fixture_token_seen || req->partial_token_seen || req->tokens_seen ||
        state->token_index || req->segment) {
        graph_arg_error2(err,
            "yvex: %s cannot be combined with %s",
            mode, layers_mode
                ? "model graph, segment, token, or standalone op options"
                : "model graph, segment, or standalone op options");
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

/* Validate one standalone primitive execution request. */
/* Purpose: Validate graph validate op mode before downstream use (`graph_validate_op_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_validate_op_mode(yvex_graph_report_request *req,
                                  const graph_parse_state *state,
                                  yvex_error *err)
{
    if (req->model) {
        graph_arg_error(err, "yvex: --execute-op does not take a model artifact");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!req->op || (strcmp(req->op, "rope") != 0 &&
                     strcmp(req->op, "attention") != 0 &&
                     strcmp(req->op, "matmul") != 0 && strcmp(req->op, "mlp") != 0)) {
        graph_arg_error(err,
            "yvex: --execute-op requires --op rope, --op attention, --op matmul, or --op mlp");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "matmul") == 0) {
        if (!state->matmul_m || !state->matmul_k || !state->matmul_n) {
            graph_arg_error(err,
                "yvex: --execute-op --op matmul requires --m M --k K --n N");
            return YVEX_ERR_INVALID_ARG;
        }
        if (state->rope_position || state->rope_head_dim ||
            state->attention_seq_len || req->causal) {
            graph_arg_error(err,
                "yvex: --position, --head-dim, --seq-len, and --causal require --op rope or --op attention");
            return YVEX_ERR_INVALID_ARG;
        }
    } else if (strcmp(req->op, "mlp") == 0) {
        if (!state->mlp_hidden_dim || !state->mlp_ffn_dim ||
            !state->mlp_activation || !req->gated) {
            graph_arg_error(err,
                "yvex: --execute-op --op mlp requires --hidden-dim N --ffn-dim N --activation silu --gated");
            return YVEX_ERR_INVALID_ARG;
        }
        if (state->rope_position || state->rope_head_dim || state->attention_seq_len ||
            req->causal || state->matmul_m || state->matmul_k || state->matmul_n) {
            graph_arg_error(err,
                "yvex: --op mlp cannot use --position, --head-dim, --seq-len, --causal, --m, --k, or --n");
            return YVEX_ERR_INVALID_ARG;
        }
        if (state->mlp_experts != state->mlp_expert_id) {
            graph_arg_error(err, "yvex: --experts and --expert-id must be provided together");
            return YVEX_ERR_INVALID_ARG;
        }
    } else if (!state->rope_position) {
        graph_arg_error(err, "yvex: --execute-op requires --position N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "matmul") != 0 && strcmp(req->op, "mlp") != 0 &&
        !state->rope_head_dim) {
        graph_arg_error(err, "yvex: --execute-op requires --head-dim N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "attention") == 0 && !state->attention_seq_len) {
        graph_arg_error(err,
            "yvex: --execute-op --op attention requires --seq-len N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "rope") == 0 && (state->attention_seq_len || req->causal)) {
        graph_arg_error(err, "yvex: --seq-len and --causal require --op attention");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "rope") != 0 && strcmp(req->op, "attention") != 0 &&
        (state->rope_position || state->rope_head_dim)) {
        graph_arg_error(err,
            "yvex: --position and --head-dim require --op rope or --op attention");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "matmul") != 0 &&
        (state->matmul_m || state->matmul_k || state->matmul_n)) {
        graph_arg_error(err, "yvex: --m, --k, and --n require --op matmul");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->op, "mlp") != 0 &&
        (state->mlp_hidden_dim || state->mlp_ffn_dim || state->mlp_activation ||
         req->gated || state->mlp_experts || state->mlp_expert_id)) {
        graph_arg_error(err,
            "yvex: --hidden-dim, --ffn-dim, --activation, --gated, --experts, and --expert-id require --op mlp");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->fixture_token_seen || req->partial_token_seen || req->tokens_seen ||
        state->token_index || req->segment) {
        graph_arg_error(err,
            "yvex: --execute-op cannot be combined with model graph token or segment options");
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

/* Validate model-bound fixture, partial, and segment execution modes. */
/* Purpose: Validate graph validate model mode before downstream use (`graph_validate_model_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_validate_model_mode(yvex_graph_report_request *req,
                                     const graph_parse_state *state,
                                     yvex_error *err)
{
    if (state->block_name) {
        graph_arg_error(err, "yvex: --block requires --execute-block or --execute-layers");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->op || state->rope_position || state->rope_head_dim ||
        state->attention_seq_len || req->causal || state->matmul_m ||
        state->matmul_k || state->matmul_n || state->mlp_hidden_dim ||
        state->mlp_ffn_dim || state->mlp_activation || req->gated ||
        state->mlp_experts || state->mlp_expert_id || state->layers) {
        graph_arg_error(err, "yvex: --op and standalone op options require --execute-op");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->execute_segment) {
        if (!req->segment) {
            graph_arg_error(err,
                "yvex: --execute-segment requires --segment embedding-rmsnorm");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->segment, "embedding-rmsnorm") != 0) {
            graph_arg_errorf(err, "yvex: unsupported segment: %s", req->segment);
            return YVEX_ERR_INVALID_ARG;
        }
    } else if (req->segment) {
        graph_arg_error(err, "yvex: --segment requires --execute-segment");
        return YVEX_ERR_INVALID_ARG;
    }
    if (state->token_index && !req->tokens_seen) {
        graph_arg_error(err, "yvex: --token-index requires --tokens");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->tokens_seen &&
        !(req->execute_fixture || req->execute_partial || req->execute_segment)) {
        graph_arg_error(err,
            "yvex: --tokens is only supported with graph execution flags");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->tokens_seen && (req->partial_token_seen || req->fixture_token_seen)) {
        graph_arg_error(err,
            "yvex: --tokens cannot be combined with --partial-token or --fixture-token");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->execute_fixture) req->action = YVEX_GRAPH_ACTION_EXECUTE_FIXTURE;
    else if (req->execute_segment) req->action = YVEX_GRAPH_ACTION_EXECUTE_SEGMENT;
    else if (req->execute_partial) req->action = YVEX_GRAPH_ACTION_EXECUTE_PARTIAL;
    return YVEX_OK;
}

/* Purpose: Validate graph validate modes before downstream use (`graph_validate_modes`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_validate_modes(yvex_graph_report_request *req,
                                const graph_parse_state *state,
                                yvex_error *err)
{
    if (!req->model && !req->execute_op && !req->execute_block && !req->execute_layers) {
        graph_arg_error(err, "yvex: graph requires FILE_OR_ALIAS");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!cli_backend_name_valid(req->backend)) {
        graph_arg_errorf(err, "yvex: unknown backend kind: %s", req->backend);
        return YVEX_ERR_INVALID_ARG;
    }
    if (graph_mode_count(req) > 1) {
        graph_arg_error(err, "yvex: graph execution flags are mutually exclusive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->execute_layers) return graph_validate_block_mode(req, state, 1, err);
    if (req->execute_block) return graph_validate_block_mode(req, state, 0, err);
    if (req->execute_op) return graph_validate_op_mode(req, state, err);
    return graph_validate_model_mode(req, state, err);
}

/* Parse the fixture-only graph-check preset. */
/* Purpose: Parse graph parse check into typed CLI state (`graph_parse_check`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_parse_check(int argc, char **argv,
                             yvex_graph_args *out, yvex_error *err)
{
    yvex_graph_report_request *req = &out->request;
    int i;

    req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    req->action = YVEX_GRAPH_ACTION_CHECK;
    req->suite = "all";
    req->layers = 2ull;
    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: graph check --backend requires cpu|cuda",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->backend = argv[++i];
        } else if (strcmp(argv[i], "--suite") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: graph check --suite requires primitives, block, layers, or all",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->suite = argv[++i];
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !parse_positive_ull(argv[i + 1], &req->layers)) {
                graph_arg_error(err,
                    "yvex: graph check --layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            if (req->layers > 16ull) {
                graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --output requires normal|table|audit",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            if (!graph_parse_output_mode(argv[++i], &out->render_mode)) {
                graph_arg_errorf(err,
                    "yvex: unsupported graph output mode: %s",
                    argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
            req->mode = out->render_mode;
        } else if (strcmp(argv[i], "--audit") == 0) {
            out->render_mode = YVEX_GRAPH_REPORT_MODE_AUDIT;
            req->mode = out->render_mode;
        } else if (strcmp(argv[i], "--model") == 0) {
            graph_arg_error(err,
                "yvex: graph check preset is fixture-only in GRAPH.CHECK.0");
            return YVEX_ERR_INVALID_ARG;
        } else if (argv[i][0] == '-') {
            graph_arg_errorf(err,
                "yvex: unknown graph check option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        } else {
            graph_arg_error(err,
                "yvex: graph check preset is fixture-only in GRAPH.CHECK.0");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    return graph_validate_check(req, err);

}
/* Parse model identity and graph execution-mode selectors. */
/* Purpose: Parse graph parse primary option into typed CLI state (`graph_parse_primary_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_parse_primary_option(int argc, char **argv, int *index,
                                      yvex_graph_args *out,
                                      graph_parse_state *state,
                                      yvex_error *err)
{
    yvex_graph_report_request *req = &out->request;
    int i = *index;

    if (strcmp(argv[i], "--model") == 0) {
        if (!graph_require_value(argc, i,
                                 "yvex: --model requires FILE_OR_ALIAS",
                                 err)) {
            return -1;
        }
        req->model = argv[++i];
    } else if (strcmp(argv[i], "--seq") == 0) {
        if (i + 1 >= argc ||
            !parse_positive_ull(argv[i + 1],
                                      &req->sequence_length)) {
            graph_arg_error(err, "yvex: --seq requires a positive integer");
            return -1;
        }
        i += 1;
    } else if (strcmp(argv[i], "--ctx") == 0) {
        if (i + 1 >= argc ||
            !parse_positive_ull(argv[i + 1],
                                      &req->context_length)) {
            graph_arg_error(err, "yvex: --ctx requires a positive integer");
            return -1;
        }
        i += 1;
    } else if (strcmp(argv[i], "--backend") == 0) {
        if (!graph_require_value(argc, i,
                                 "yvex: --backend requires cpu|cuda",
                                 err)) {
            return -1;
        }
        req->backend = argv[++i];
    } else if (strcmp(argv[i], "--execute-fixture") == 0) {
        req->execute_fixture = 1;
    } else if (strcmp(argv[i], "--fixture-token") == 0) {
        unsigned int value;
        if (i + 1 >= argc ||
            !parse_uint_allow_zero(argv[i + 1], &value)) {
            graph_arg_error(err,
                "yvex: --fixture-token requires a non-negative integer");
            return -1;
        }
        req->fixture_token = value;
        req->fixture_token_seen = 1;
        i += 1;
    } else if (strcmp(argv[i], "--execute-partial") == 0) {
        req->execute_partial = 1;
    } else if (strcmp(argv[i], "--execute-segment") == 0) {
        req->execute_segment = 1;
    } else if (strcmp(argv[i], "--execute-op") == 0) {
        req->execute_op = 1;
        req->action = YVEX_GRAPH_ACTION_EXECUTE_OP;
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    } else if (strcmp(argv[i], "--execute-block") == 0) {
        req->execute_block = 1;
        req->action = YVEX_GRAPH_ACTION_EXECUTE_BLOCK;
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    } else if (strcmp(argv[i], "--execute-layers") == 0) {
        req->execute_layers = 1;
        req->action = YVEX_GRAPH_ACTION_EXECUTE_LAYERS;
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    } else if (strcmp(argv[i], "--op") == 0) {
        if (!graph_require_value(argc, i,
                                 "yvex: --op requires rope, attention, matmul, or mlp",
                                 err)) {
            return -1;
        }
        req->op = argv[++i];
    } else if (strcmp(argv[i], "--block") == 0) {
        if (!graph_require_value(argc, i,
                                 "yvex: --block requires fixture",
                                 err)) {
            return -1;
        }
        req->block = argv[++i];
        state->block_name = 1;

    } else {
        return 0;
    }
    *index = i;
    return 1;
}

/* Parse standalone primitive geometry and model-bound token options. */
/* Purpose: Parse graph parse detail option into typed CLI state (`graph_parse_detail_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_parse_detail_option(int argc, char **argv, int *index,
                                     yvex_graph_args *out,
                                     graph_parse_state *state,
                                     yvex_error *err)
{
    const graph_detail_spec *spec = graph_detail_find(argv[*index]);

    if (spec) return graph_detail_bind(spec, argc, argv, index, out, state, err);
    if (!out->request.model) {
        out->request.model = argv[*index];
        return 1;
    }
    graph_arg_errorf(err, "yvex: unknown graph option: %s", argv[*index]);
    return -1;
}
/* Purpose: Parse graph args parse into typed CLI state (`yvex_graph_args_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_args_parse(int argc,
                          char **argv,
                          yvex_graph_args *out,
                          yvex_error *err)
{
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

    for (i = 2; i < argc; ++i) {
        int handled = graph_parse_primary_option(argc, argv, &i, out, &state, err);
        if (handled < 0) return YVEX_ERR_INVALID_ARG;
        if (!handled) {
            handled = graph_parse_detail_option(argc, argv, &i, out, &state, err);
            if (handled < 0) return YVEX_ERR_INVALID_ARG;
        }
    }
    if (req->execute_op || req->execute_block || req->execute_layers) {
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    }
    return graph_validate_modes(req, &state, err);
}
