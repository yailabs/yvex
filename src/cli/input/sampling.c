/* Owner: src/cli/input
 * Owns: CLI grammar and value validation for the sampling command.
 * Does not own: model reference resolution, graph guard preflight, engine open, sampler execution, report
 *   construction, rendering, stdout/stderr output, generation, eval, benchmark, or release decisions.
 * Invariants: the parser only translates borrowed argv into a typed request.
 * Boundary: input parsing is not bounded sampling execution.
 * Purpose: provide cLI grammar and value validation for the sampling command.
 * Inputs: bounded command arguments and caller-owned typed request storage.
 * Effects: publishes request fields only after complete grammar validation.
 * Failure: invalid or ambiguous grammar leaves the request uncommitted. */
#include "src/cli/input/private.h"

#include <string.h>

/* Purpose: Compute sampling arg error for its CLI invariant (`sampling_arg_error`). */
static void sampling_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "sample", message);
}

/* Purpose: Compute sampling arg errorf for its CLI invariant (`sampling_arg_errorf`). */
static void sampling_arg_errorf(yvex_error *err,
                                const char *fmt,
                                const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample", fmt,
                    value ? value : "");
}

/* Purpose: Parse sample parse strategy into typed CLI state (`sample_parse_strategy`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int sample_parse_strategy(const char *text,
                                 yvex_sampling_strategy *strategy)
{
    if (!text || !strategy) {
        return 0;
    }
    if (strcmp(text, "greedy") == 0) {
        *strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
        return 1;
    }
    return 0;
}

/* Purpose: Compute sample logits count valid for its CLI invariant (`sample_logits_count_valid`). */
static int sample_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

/* Purpose: Compute sampling args defaults for its CLI invariant (`sampling_args_defaults`). */
static void sampling_args_defaults(yvex_sampling_args *out)
{
    memset(out, 0, sizeof(*out));
    out->request.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
    out->request.logits_count = 16ull;
    out->render_mode = YVEX_SAMPLING_REPORT_NORMAL;
    out->help_exit_code = 0;
}

typedef enum sampling_option_kind {
    SAMPLING_OPTION_TEXT,
    SAMPLING_OPTION_POSITIVE,
    SAMPLING_OPTION_NONNEGATIVE,
    SAMPLING_OPTION_LOGITS,
    SAMPLING_OPTION_STRATEGY,
    SAMPLING_OPTION_FLAG
} sampling_option_kind;

typedef struct sampling_option_spec {
    const char *flag;
    sampling_option_kind kind;
    size_t offset;
    unsigned int seen_mask;
    const char *error;
} sampling_option_spec;

enum {
    SAMPLING_SEEN_KV = 1u << 0u,
    SAMPLING_SEEN_LAYERS = 1u << 1u,
    SAMPLING_SEEN_HIDDEN = 1u << 2u,
    SAMPLING_SEEN_HEAD = 1u << 3u,
    SAMPLING_SEEN_FFN = 1u << 4u,
    SAMPLING_SEEN_CHUNK = 1u << 5u,
    SAMPLING_SEEN_CONTEXT = 1u << 6u
};

#define SAMPLE_FIELD(member_) offsetof(yvex_sampling_report_request, member_)
#define SAMPLE_KV_FIELD(member_) \
    (offsetof(yvex_sampling_report_request, kv_shape) + offsetof(yvex_kv_shape, member_))

static const sampling_option_spec sampling_options[] = {
    {"--model", SAMPLING_OPTION_TEXT, SAMPLE_FIELD(model_arg), 0u,
     "yvex: --model requires FILE_OR_ALIAS"},
    {"--backend", SAMPLING_OPTION_TEXT, SAMPLE_FIELD(backend_name), 0u,
     "yvex: --backend requires cpu|cuda"},
    {"--segment", SAMPLING_OPTION_TEXT, SAMPLE_FIELD(segment_name), 0u,
     "yvex: --segment requires embedding-rmsnorm"},
    {"--tokens", SAMPLING_OPTION_TEXT, SAMPLE_FIELD(tokens_text), 0u,
     "yvex: --tokens requires IDS"},
    {"--strategy", SAMPLING_OPTION_STRATEGY, SAMPLE_FIELD(strategy), 0u,
     "yvex: --strategy supports only greedy"},
    {"--logits-count", SAMPLING_OPTION_LOGITS, SAMPLE_FIELD(logits_count), 0u,
     "yvex: --logits-count requires 1 <= N <= 256"},
    {"--attach-kv", SAMPLING_OPTION_FLAG, SAMPLE_FIELD(attach_kv), 0u, NULL},
    {"--kv-layers", SAMPLING_OPTION_POSITIVE, SAMPLE_KV_FIELD(layer_count),
     SAMPLING_SEEN_KV, "yvex: --kv-layers requires a positive integer"},
    {"--kv-heads", SAMPLING_OPTION_POSITIVE, SAMPLE_KV_FIELD(kv_head_count),
     SAMPLING_SEEN_KV, "yvex: --kv-heads requires a positive integer"},
    {"--kv-head-dim", SAMPLING_OPTION_POSITIVE, SAMPLE_KV_FIELD(head_dim),
     SAMPLING_SEEN_KV, "yvex: --kv-head-dim requires a positive integer"},
    {"--kv-capacity", SAMPLING_OPTION_POSITIVE, SAMPLE_KV_FIELD(capacity),
     SAMPLING_SEEN_KV, "yvex: --kv-capacity requires a positive integer"},
    {"--layers", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(layer_count),
     SAMPLING_SEEN_LAYERS, "yvex: --layers requires a positive integer"},
    {"--layer-hidden-dim", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(layer_hidden_dim),
     SAMPLING_SEEN_HIDDEN, "yvex: --layer-hidden-dim requires a positive integer"},
    {"--layer-head-dim", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(layer_head_dim),
     SAMPLING_SEEN_HEAD, "yvex: --layer-head-dim requires a positive integer"},
    {"--layer-ffn-dim", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(layer_ffn_dim),
     SAMPLING_SEEN_FFN, "yvex: --layer-ffn-dim requires a positive integer"},
    {"--chunk-size", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(chunk_size),
     SAMPLING_SEEN_CHUNK, "yvex: --chunk-size requires a positive integer"},
    {"--position-start", SAMPLING_OPTION_NONNEGATIVE, SAMPLE_FIELD(position_start), 0u,
     "yvex: --position-start requires a non-negative integer"},
    {"--context-length", SAMPLING_OPTION_POSITIVE, SAMPLE_FIELD(context_length),
     SAMPLING_SEEN_CONTEXT, "yvex: --context-length requires a positive integer"},
};

#undef SAMPLE_KV_FIELD
#undef SAMPLE_FIELD

/* Find one admitted sampling option without making argv spelling authoritative. */
/* Purpose: Compute sampling option find for its CLI invariant (`sampling_option_find`). */
static const sampling_option_spec *sampling_option_find(const char *flag)
{
    size_t i;

    for (i = 0; i < sizeof(sampling_options) / sizeof(sampling_options[0]); ++i) {
        if (strcmp(flag, sampling_options[i].flag) == 0) return &sampling_options[i];
    }
    return NULL;
}

/* Bind one validated option into the typed sampling request. */
/* Purpose: Compute sampling option bind for its CLI invariant (`sampling_option_bind`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int sampling_option_bind(const sampling_option_spec *spec,
                                int argc,
                                char **argv,
                                int *index,
                                yvex_sampling_report_request *request,
                                yvex_error *err)
{
    unsigned char *field = (unsigned char *)request + spec->offset;
    const char *value;

    if (spec->kind == SAMPLING_OPTION_FLAG) {
        *(int *)field = 1;
        return YVEX_OK;
    }
    if (*index + 1 >= argc) {
        sampling_arg_error(err, spec->error);
        return YVEX_ERR_INVALID_ARG;
    }
    value = argv[++*index];
    if (spec->kind == SAMPLING_OPTION_TEXT) {
        *(const char **)field = value;
        return YVEX_OK;
    }
    if (spec->kind == SAMPLING_OPTION_STRATEGY) {
        if (sample_parse_strategy(value, (yvex_sampling_strategy *)field)) return YVEX_OK;
    } else if (spec->kind == SAMPLING_OPTION_POSITIVE) {
        if (parse_positive_ull(value, (unsigned long long *)field)) return YVEX_OK;
    } else if (spec->kind == SAMPLING_OPTION_NONNEGATIVE) {
        if (parse_ull_allow_zero(value, (unsigned long long *)field)) return YVEX_OK;
    } else if (parse_positive_ull(value, (unsigned long long *)field) &&
               sample_logits_count_valid(*(unsigned long long *)field)) {
        return YVEX_OK;
    }
    sampling_arg_error(err, spec->error);
    return YVEX_ERR_INVALID_ARG;
}

/* Purpose: Validate sampling args validate shape before downstream use (`sampling_args_validate_shape`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int sampling_args_validate_shape(const yvex_sampling_report_request *req,
                                        int kv_shape_seen,
                                        int layer_hidden_seen,
                                        int layer_head_seen,
                                        int layer_ffn_seen,
                                        yvex_error *err)
{
    if (!req->model_arg || !req->backend_name || !req->tokens_text ||
        !req->segment_name) {
        sampling_arg_error(err,
            "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --"
                "tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-"
                "size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --"
                "kv-head-dim N --kv-capacity N] [--logits-count N] [--strategy greedy]");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!cli_backend_name_valid(req->backend_name)) {
        sampling_arg_errorf(err, "yvex: unknown backend kind: %s",
                            req->backend_name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->segment_name, "embedding-rmsnorm") != 0) {
        sampling_arg_errorf(err, "yvex: unsupported sample segment: %s",
                            req->segment_name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv_shape_seen && !req->attach_kv) {
        sampling_arg_error(err, "yvex: --kv-* options require --attach-kv");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->attach_kv && (req->kv_shape.layer_count == 0ull ||
                           req->kv_shape.kv_head_count == 0ull ||
                           req->kv_shape.head_dim == 0ull ||
                           req->kv_shape.capacity == 0ull)) {
        sampling_arg_error(err,
            "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!req->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        sampling_arg_error(err, "yvex: --layer-* options require --layers N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        sampling_arg_error(err,
            "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        (req->layer_count == 0ull || req->layer_count > 16ull)) {
        sampling_arg_error(err, "yvex: --layers requires 1 <= N <= 16");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        req->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample",
            "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff",
            (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

/* Purpose: Compute sampling args finish for its CLI invariant (`sampling_args_finish`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int sampling_args_finish(yvex_sampling_report_request *req,
                                int kv_shape_seen,
                                int layer_hidden_seen,
                                int layer_head_seen,
                                int layer_ffn_seen,
                                yvex_error *err)
{
    int rc;

    if (req->layer_count_seen && !layer_hidden_seen) {
        req->layer_hidden_dim = 8ull;
        req->layer_head_dim = 8ull;
        req->layer_ffn_dim = 16ull;
    }
    rc = sampling_args_validate_shape(req,
                                      kv_shape_seen,
                                      layer_hidden_seen,
                                      layer_head_seen,
                                      layer_ffn_seen,
                                      err);
    if (rc == YVEX_OK) {
        yvex_error_clear(err);
    }
    return rc;
}

/* Purpose: Parse sampling args parse into typed CLI state (`yvex_sampling_args_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_sampling_args_parse(int argc, char **argv,
                             yvex_sampling_args *out, yvex_error *err)
{
    yvex_sampling_report_request *req;
    unsigned int seen = 0u;
    int i;
    int rc;

    if (!out) {
        sampling_arg_error(err, "sample parser requires output");
        return YVEX_ERR_INVALID_ARG;
    }
    sampling_args_defaults(out);
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

    for (i = 2; i < argc; ++i) {
        const sampling_option_spec *spec = sampling_option_find(argv[i]);

        if (!spec) {
            yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample",
                            "yvex: unknown sample option: %s\nTry 'yvex help sample' for usage.",
                            argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
        rc = sampling_option_bind(spec, argc, argv, &i, req, err);
        if (rc != YVEX_OK) return rc;
        seen |= spec->seen_mask;
    }

    req->layer_count_seen = (seen & SAMPLING_SEEN_LAYERS) != 0u;
    req->chunk_size_seen = (seen & SAMPLING_SEEN_CHUNK) != 0u;
    req->context_length_seen = (seen & SAMPLING_SEEN_CONTEXT) != 0u;
    rc = sampling_args_finish(req, (seen & SAMPLING_SEEN_KV) != 0u,
                              (seen & SAMPLING_SEEN_HIDDEN) != 0u,
                              (seen & SAMPLING_SEEN_HEAD) != 0u,
                              (seen & SAMPLING_SEEN_FFN) != 0u, err);
    return rc;
}
