/*
 * yvex_graph_args.c - graph command argv parsing.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   CLI grammar, option defaults, and parser validation for `yvex graph`.
 *
 * Does not own:
 *   model reference resolution, artifact inspection, graph construction,
 *   backend probing, primitive execution, report building, rendering,
 *   stdout/stderr output, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   this parser only reads borrowed argc/argv and fills a typed request.
 *
 * Boundary:
 *   parsing graph options is not graph runtime support.
 */
#include "yvex_graph_args.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void graph_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph", message);
}

static void graph_arg_errorf(yvex_error *err,
                             const char *fmt,
                             const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "graph", fmt,
                    value ? value : "");
}

static int graph_parse_ull_raw(const char *text,
                               unsigned long long *out,
                               int allow_zero)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        return 0;
    }
    if (!allow_zero && value == 0ull) {
        return 0;
    }
    *out = value;
    return 1;
}

static int graph_parse_positive_ull(const char *text,
                                    unsigned long long *out)
{
    return graph_parse_ull_raw(text, out, 0);
}

static int graph_parse_ull_allow_zero(const char *text,
                                      unsigned long long *out)
{
    return graph_parse_ull_raw(text, out, 1);
}

static int graph_parse_uint_allow_zero(const char *text,
                                       unsigned int *out)
{
    unsigned long long value;

    if (!graph_parse_ull_allow_zero(text, &value) || value > 4294967295ull) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

static int graph_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static int graph_mode_count(const yvex_graph_report_request *req)
{
    return (req->execute_fixture ? 1 : 0) +
           (req->execute_partial ? 1 : 0) +
           (req->execute_segment ? 1 : 0) +
           (req->execute_op ? 1 : 0) +
           (req->execute_block ? 1 : 0) +
           (req->execute_layers ? 1 : 0);
}

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

static int graph_validate_check(yvex_graph_report_request *req,
                                yvex_error *err)
{
    if (!graph_backend_name_valid(req->backend)) {
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

static int graph_validate_modes(yvex_graph_report_request *req,
                                int block_name_provided,
                                int token_index_provided,
                                int rope_position_provided,
                                int rope_head_dim_provided,
                                int attention_seq_len_provided,
                                int matmul_m_provided,
                                int matmul_k_provided,
                                int matmul_n_provided,
                                int mlp_hidden_dim_provided,
                                int mlp_ffn_dim_provided,
                                int mlp_activation_provided,
                                int mlp_experts_provided,
                                int mlp_expert_id_provided,
                                int layers_provided,
                                yvex_error *err)
{
    if (!req->model && !req->execute_op && !req->execute_block &&
        !req->execute_layers) {
        graph_arg_error(err, "yvex: graph requires FILE_OR_ALIAS");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!graph_backend_name_valid(req->backend)) {
        graph_arg_errorf(err, "yvex: unknown backend kind: %s", req->backend);
        return YVEX_ERR_INVALID_ARG;
    }
    if (graph_mode_count(req) > 1) {
        graph_arg_error(err,
            "yvex: graph execution flags are mutually exclusive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->execute_layers) {
        if (req->model) {
            graph_arg_error(err,
                "yvex: --execute-layers does not take a model artifact");
            return YVEX_ERR_INVALID_ARG;
        }
        if (!block_name_provided) {
            graph_arg_error(err,
                "yvex: --execute-layers requires --block fixture");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->block, "fixture") != 0) {
            graph_arg_errorf(err, "yvex: unsupported block: %s", req->block);
            return YVEX_ERR_INVALID_ARG;
        }
        if (!layers_provided) {
            graph_arg_error(err, "yvex: --execute-layers requires --layers N");
            return YVEX_ERR_INVALID_ARG;
        }
        if (!attention_seq_len_provided || !rope_position_provided ||
            !mlp_hidden_dim_provided || !rope_head_dim_provided ||
            !mlp_ffn_dim_provided) {
            graph_arg_error(err,
                "yvex: --execute-layers requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N");
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
        if (req->op || matmul_m_provided || matmul_k_provided ||
            matmul_n_provided || mlp_activation_provided ||
            mlp_experts_provided || mlp_expert_id_provided ||
            req->fixture_token_seen || req->partial_token_seen ||
            req->tokens_seen || token_index_provided || req->segment) {
            graph_arg_error(err,
                "yvex: --execute-layers cannot be combined with model graph, segment, token, or standalone op options");
            return YVEX_ERR_INVALID_ARG;
        }
    } else if (req->execute_block) {
        if (req->model) {
            graph_arg_error(err,
                "yvex: --execute-block does not take a model artifact");
            return YVEX_ERR_INVALID_ARG;
        }
        if (!block_name_provided) {
            graph_arg_error(err,
                "yvex: --execute-block requires --block fixture");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->block, "fixture") != 0) {
            graph_arg_errorf(err, "yvex: unsupported block: %s", req->block);
            return YVEX_ERR_INVALID_ARG;
        }
        if (!attention_seq_len_provided || !rope_position_provided ||
            !mlp_hidden_dim_provided || !rope_head_dim_provided ||
            !mlp_ffn_dim_provided) {
            graph_arg_error(err,
                "yvex: --execute-block requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N");
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
        if (req->op || matmul_m_provided || matmul_k_provided ||
            matmul_n_provided || mlp_activation_provided ||
            mlp_experts_provided || mlp_expert_id_provided ||
            req->fixture_token_seen || req->partial_token_seen ||
            req->tokens_seen || token_index_provided || req->segment) {
            graph_arg_error(err,
                "yvex: --execute-block cannot be combined with model graph, segment, or standalone op options");
            return YVEX_ERR_INVALID_ARG;
        }
    } else if (req->execute_op) {
        if (req->model) {
            graph_arg_error(err,
                "yvex: --execute-op does not take a model artifact");
            return YVEX_ERR_INVALID_ARG;
        }
        if (!req->op ||
            (strcmp(req->op, "rope") != 0 &&
             strcmp(req->op, "attention") != 0 &&
             strcmp(req->op, "matmul") != 0 &&
             strcmp(req->op, "mlp") != 0)) {
            graph_arg_error(err,
                "yvex: --execute-op requires --op rope, --op attention, --op matmul, or --op mlp");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "matmul") == 0) {
            if (!matmul_m_provided || !matmul_k_provided ||
                !matmul_n_provided) {
                graph_arg_error(err,
                    "yvex: --execute-op --op matmul requires --m M --k K --n N");
                return YVEX_ERR_INVALID_ARG;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || req->causal) {
                graph_arg_error(err,
                    "yvex: --position, --head-dim, --seq-len, and --causal require --op rope or --op attention");
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(req->op, "mlp") == 0) {
            if (!mlp_hidden_dim_provided || !mlp_ffn_dim_provided ||
                !mlp_activation_provided || !req->gated) {
                graph_arg_error(err,
                    "yvex: --execute-op --op mlp requires --hidden-dim N --ffn-dim N --activation silu --gated");
                return YVEX_ERR_INVALID_ARG;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || req->causal ||
                matmul_m_provided || matmul_k_provided ||
                matmul_n_provided) {
                graph_arg_error(err,
                    "yvex: --op mlp cannot use --position, --head-dim, --seq-len, --causal, --m, --k, or --n");
                return YVEX_ERR_INVALID_ARG;
            }
            if (mlp_experts_provided != mlp_expert_id_provided) {
                graph_arg_error(err,
                    "yvex: --experts and --expert-id must be provided together");
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (!rope_position_provided) {
            graph_arg_error(err, "yvex: --execute-op requires --position N");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "matmul") != 0 &&
            strcmp(req->op, "mlp") != 0 && !rope_head_dim_provided) {
            graph_arg_error(err, "yvex: --execute-op requires --head-dim N");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "attention") == 0 &&
            !attention_seq_len_provided) {
            graph_arg_error(err,
                "yvex: --execute-op --op attention requires --seq-len N");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "rope") == 0 &&
            (attention_seq_len_provided || req->causal)) {
            graph_arg_error(err, "yvex: --seq-len and --causal require --op attention");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "rope") != 0 &&
            strcmp(req->op, "attention") != 0 &&
            (rope_position_provided || rope_head_dim_provided)) {
            graph_arg_error(err,
                "yvex: --position and --head-dim require --op rope or --op attention");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "matmul") != 0 &&
            (matmul_m_provided || matmul_k_provided || matmul_n_provided)) {
            graph_arg_error(err, "yvex: --m, --k, and --n require --op matmul");
            return YVEX_ERR_INVALID_ARG;
        }
        if (strcmp(req->op, "mlp") != 0 &&
            (mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
             mlp_activation_provided || req->gated || mlp_experts_provided ||
             mlp_expert_id_provided)) {
            graph_arg_error(err,
                "yvex: --hidden-dim, --ffn-dim, --activation, --gated, --experts, and --expert-id require --op mlp");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->fixture_token_seen || req->partial_token_seen ||
            req->tokens_seen || token_index_provided || req->segment) {
            graph_arg_error(err,
                "yvex: --execute-op cannot be combined with model graph token or segment options");
            return YVEX_ERR_INVALID_ARG;
        }
    } else {
        if (block_name_provided) {
            graph_arg_error(err,
                "yvex: --block requires --execute-block or --execute-layers");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->op || rope_position_provided || rope_head_dim_provided ||
            attention_seq_len_provided || req->causal ||
            matmul_m_provided || matmul_k_provided || matmul_n_provided ||
            mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
            mlp_activation_provided || req->gated || mlp_experts_provided ||
            mlp_expert_id_provided || layers_provided) {
            graph_arg_error(err,
                "yvex: --op and standalone op options require --execute-op");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->execute_segment) {
            if (!req->segment) {
                graph_arg_error(err,
                    "yvex: --execute-segment requires --segment embedding-rmsnorm");
                return YVEX_ERR_INVALID_ARG;
            }
            if (strcmp(req->segment, "embedding-rmsnorm") != 0) {
                graph_arg_errorf(err, "yvex: unsupported segment: %s",
                                 req->segment);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (req->segment) {
            graph_arg_error(err, "yvex: --segment requires --execute-segment");
            return YVEX_ERR_INVALID_ARG;
        }
        if (token_index_provided && !req->tokens_seen) {
            graph_arg_error(err, "yvex: --token-index requires --tokens");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->tokens_seen &&
            !(req->execute_fixture || req->execute_partial ||
              req->execute_segment)) {
            graph_arg_error(err,
                "yvex: --tokens is only supported with graph execution flags");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->tokens_seen &&
            (req->partial_token_seen || req->fixture_token_seen)) {
            graph_arg_error(err,
                "yvex: --tokens cannot be combined with --partial-token or --fixture-token");
            return YVEX_ERR_INVALID_ARG;
        }
        if (req->execute_fixture) {
            req->action = YVEX_GRAPH_ACTION_EXECUTE_FIXTURE;
        } else if (req->execute_segment) {
            req->action = YVEX_GRAPH_ACTION_EXECUTE_SEGMENT;
        } else if (req->execute_partial) {
            req->action = YVEX_GRAPH_ACTION_EXECUTE_PARTIAL;
        }
    }
    return YVEX_OK;
}

int yvex_graph_args_parse(int argc,
                          char **argv,
                          yvex_graph_args *out,
                          yvex_error *err)
{
    yvex_graph_report_request *req;
    int block_name_provided = 0;
    int token_index_provided = 0;
    int rope_position_provided = 0;
    int rope_head_dim_provided = 0;
    int attention_seq_len_provided = 0;
    int matmul_m_provided = 0;
    int matmul_k_provided = 0;
    int matmul_n_provided = 0;
    int mlp_hidden_dim_provided = 0;
    int mlp_ffn_dim_provided = 0;
    int mlp_activation_provided = 0;
    int mlp_experts_provided = 0;
    int mlp_expert_id_provided = 0;
    int layers_provided = 0;
    int i;

    if (!out) {
        graph_arg_error(err, "graph parser requires output");
        return YVEX_ERR_INVALID_ARG;
    }
    graph_args_defaults(out);
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
                    !graph_parse_positive_ull(argv[i + 1], &req->layers)) {
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

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --model requires FILE_OR_ALIAS",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->model = argv[++i];
        } else if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1],
                                          &req->sequence_length)) {
                graph_arg_error(err, "yvex: --seq requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1],
                                          &req->context_length)) {
                graph_arg_error(err, "yvex: --ctx requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --backend requires cpu|cuda",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->backend = argv[++i];
        } else if (strcmp(argv[i], "--execute-fixture") == 0) {
            req->execute_fixture = 1;
        } else if (strcmp(argv[i], "--fixture-token") == 0) {
            unsigned int value;
            if (i + 1 >= argc ||
                !graph_parse_uint_allow_zero(argv[i + 1], &value)) {
                graph_arg_error(err,
                    "yvex: --fixture-token requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
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
                return YVEX_ERR_INVALID_ARG;
            }
            req->op = argv[++i];
        } else if (strcmp(argv[i], "--block") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --block requires fixture",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->block = argv[++i];
            block_name_provided = 1;
        } else if (strcmp(argv[i], "--m") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->m)) {
                graph_arg_error(err, "yvex: --m requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            matmul_m_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--k") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->k)) {
                graph_arg_error(err, "yvex: --k requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            matmul_k_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--n") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->n)) {
                graph_arg_error(err, "yvex: --n requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            matmul_n_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--hidden-dim") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->hidden_dim)) {
                graph_arg_error(err,
                    "yvex: --hidden-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            mlp_hidden_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--ffn-dim") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->ffn_dim)) {
                graph_arg_error(err,
                    "yvex: --ffn-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            mlp_ffn_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--activation") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --activation requires silu",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->activation = argv[++i];
            mlp_activation_provided = 1;
        } else if (strcmp(argv[i], "--gated") == 0) {
            req->gated = 1;
        } else if (strcmp(argv[i], "--experts") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->experts)) {
                graph_arg_error(err,
                    "yvex: --experts requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            mlp_experts_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--expert-id") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_ull_allow_zero(argv[i + 1], &req->expert_id)) {
                graph_arg_error(err,
                    "yvex: --expert-id requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            req->use_expert = 1;
            mlp_expert_id_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->layers)) {
                graph_arg_error(err,
                    "yvex: --layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            if (req->layers > 16ull) {
                graph_arg_error(err, "yvex: requires 1 <= --layers <= 16");
                return YVEX_ERR_INVALID_ARG;
            }
            layers_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--seq-len") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->seq_len)) {
                graph_arg_error(err,
                    "yvex: --seq-len requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            attention_seq_len_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_ull_allow_zero(argv[i + 1], &req->position)) {
                graph_arg_error(err,
                    "yvex: --position requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            rope_position_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_positive_ull(argv[i + 1], &req->head_dim)) {
                graph_arg_error(err,
                    "yvex: --head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            rope_head_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--causal") == 0) {
            req->causal = 1;
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --segment requires embedding-rmsnorm",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->segment = argv[++i];
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            unsigned int value;
            if (i + 1 >= argc ||
                !graph_parse_uint_allow_zero(argv[i + 1], &value)) {
                graph_arg_error(err,
                    "yvex: --partial-token requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            req->partial_token = value;
            req->partial_token_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --tokens requires comma-separated token IDs",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->tokens_text = argv[++i];
            req->tokens_seen = 1;
        } else if (strcmp(argv[i], "--token-index") == 0) {
            if (i + 1 >= argc ||
                !graph_parse_ull_allow_zero(argv[i + 1],
                                            &req->token_index)) {
                graph_arg_error(err,
                    "yvex: --token-index requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            token_index_provided = 1;
            req->token_index_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (!graph_require_value(argc, i,
                                     "yvex: --output requires normal|table|audit",
                                     err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            if (!graph_parse_output_mode(argv[++i], &out->render_mode)) {
                graph_arg_errorf(err,
                    "yvex: unsupported graph output mode: %s", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
            req->mode = out->render_mode;
        } else if (strcmp(argv[i], "--audit") == 0) {
            out->render_mode = YVEX_GRAPH_REPORT_MODE_AUDIT;
            req->mode = out->render_mode;
        } else if (!req->model) {
            req->model = argv[i];
        } else {
            graph_arg_errorf(err, "yvex: unknown graph option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }
    if (req->execute_op || req->execute_block || req->execute_layers) {
        req->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    }
    return graph_validate_modes(req,
                                block_name_provided,
                                token_index_provided,
                                rope_position_provided,
                                rope_head_dim_provided,
                                attention_seq_len_provided,
                                matmul_m_provided,
                                matmul_k_provided,
                                matmul_n_provided,
                                mlp_hidden_dim_provided,
                                mlp_ffn_dim_provided,
                                mlp_activation_provided,
                                mlp_experts_provided,
                                mlp_expert_id_provided,
                                layers_provided,
                                err);
}
