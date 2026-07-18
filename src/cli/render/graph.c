/*
 * graph.c - typed graph CLI rendering.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, table, audit, and help text rendering for graph reports.
 *
 * Does not own:
 *   graph construction, report building, input parsing, command dispatch,
 *   backend primitive execution, reference comparison, stdout/stderr writer
 *   primitives, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   all output goes through src/cli/io writer helpers.
 *
 * Boundary:
 *   graph rendering is not graph runtime or generation readiness.
 */
#include "graph.h"

#include "src/cli/io/out.h"

static const char *graph_render_body(const yvex_graph_report *report)
{
    return report && report->body ? report->body : "";
}

int yvex_graph_render_normal(FILE *fp,
                             const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

int yvex_graph_render_table(FILE *fp,
                            const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

int yvex_graph_render_audit(FILE *fp,
                            const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

int yvex_graph_render(FILE *fp,
                      yvex_graph_report_mode mode,
                      const yvex_graph_report *report)
{
    if (mode == YVEX_GRAPH_REPORT_MODE_TABLE) {
        return yvex_graph_render_table(fp, report);
    }
    if (mode == YVEX_GRAPH_REPORT_MODE_AUDIT) {
        return yvex_graph_render_audit(fp, report);
    }
    return yvex_graph_render_normal(fp, report);
}

int yvex_graph_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda]\n");
    yvex_cli_out_writef(fp, "       yvex graph check [--suite primitives|block|layers|all] [--backend cpu|cuda]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op mlp --hidden-dim N --ffn-dim N --activation silu --gated [--experts N --expert-id N]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-block --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-layers --layers N --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]\n");
    yvex_cli_out_writef(fp, "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-fixture [--fixture-token N]\n");
    yvex_cli_out_writef(fp, "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-partial [--partial-token N | --tokens IDS --token-index N]\n");
    yvex_cli_out_writef(fp, "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-segment --segment embedding-rmsnorm [--partial-token N | --tokens IDS --token-index N]\n");
    yvex_cli_out_writef(fp, "\n");
    yvex_cli_out_writef(fp, "example: yvex graph check --suite primitives --backend cpu\n");
    yvex_cli_out_writef(fp, "boundary: graph construction, selected primitive proof, and selected graph slice proof are not full transformer execution or generation readiness\n");
    return YVEX_OK;
}
