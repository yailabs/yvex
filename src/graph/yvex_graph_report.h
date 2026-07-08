/*
 * yvex_graph_report.h - typed graph report API.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   request and report shapes for graph, memory-plan, plan, guard, and
 *   primitive report facts.
 *
 * Does not own:
 *   CLI input parsing, command dispatch, rendering, stdout/stderr output,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   graph reports carry typed facts and optional body text only; graph report
 *   builders do not write operator output.
 *
 * Boundary:
 *   graph reports are not transformer execution or generation readiness.
 */
#ifndef YVEX_GRAPH_REPORT_H
#define YVEX_GRAPH_REPORT_H

#include <stddef.h>
#include <yvex/error.h>
#include <yvex/graph.h>
#include <yvex/yvex.h>

typedef enum {
    YVEX_GRAPH_REPORT_KIND_GRAPH = 0,
    YVEX_GRAPH_REPORT_KIND_MEMORY_PLAN,
    YVEX_GRAPH_REPORT_KIND_PLAN,
    YVEX_GRAPH_REPORT_KIND_PRIMITIVE,
    YVEX_GRAPH_REPORT_KIND_GUARD
} yvex_graph_report_kind;

typedef enum {
    YVEX_GRAPH_REPORT_MODE_NORMAL = 0,
    YVEX_GRAPH_REPORT_MODE_TABLE,
    YVEX_GRAPH_REPORT_MODE_AUDIT
} yvex_graph_report_mode;

typedef enum {
    YVEX_GRAPH_ACTION_DUMP = 0,
    YVEX_GRAPH_ACTION_CHECK,
    YVEX_GRAPH_ACTION_EXECUTE_FIXTURE,
    YVEX_GRAPH_ACTION_EXECUTE_PARTIAL,
    YVEX_GRAPH_ACTION_EXECUTE_SEGMENT,
    YVEX_GRAPH_ACTION_EXECUTE_OP,
    YVEX_GRAPH_ACTION_EXECUTE_BLOCK,
    YVEX_GRAPH_ACTION_EXECUTE_LAYERS
} yvex_graph_action;

typedef struct {
    yvex_graph_report_kind kind;
    yvex_graph_action action;
    yvex_graph_report_mode mode;

    const char *model;
    const char *backend;
    const char *segment;
    const char *tokens_text;
    const char *op;
    const char *block;
    const char *suite;
    const char *activation;

    unsigned long long sequence_length;
    unsigned long long context_length;
    unsigned long long fixture_token;
    unsigned long long partial_token;
    unsigned long long token_index;
    unsigned long long position;
    unsigned long long head_dim;
    unsigned long long seq_len;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long experts;
    unsigned long long expert_id;
    unsigned long long layers;

    int execute_fixture;
    int execute_partial;
    int execute_segment;
    int execute_op;
    int execute_block;
    int execute_layers;
    int causal;
    int gated;
    int use_expert;
    int tokens_seen;
    int fixture_token_seen;
    int partial_token_seen;
    int token_index_seen;
} yvex_graph_report_request;

typedef struct {
    yvex_graph_report_kind kind;
    const char *status;
    const char *graph_status;
    const char *architecture;
    const char *model_name;
    const char *backend;
    const char *backend_status;
    int backend_capability_tensor_alloc;
    int backend_capability_tensor_read_write;
    int backend_capability_op_embed;
    int backend_capability_op_matmul;
    int backend_capability_op_mlp;
    int backend_capability_op_rms_norm;
    int backend_capability_op_rope;
    int backend_capability_op_attention;
    unsigned long long value_count;
    unsigned long long op_count;
    unsigned long long missing_required_count;
    const char *memory_status;
    unsigned long long model_tensor_bytes_known;
    unsigned long long activation_peak_bytes;
    unsigned long long total_known_bytes;
    int execution_ready;
    const char *reason;
    const char *boundary;
    char *body;
    size_t body_len;
    size_t body_cap;
    int exit_code;
} yvex_graph_report;

int yvex_graph_report_appendf(yvex_graph_report *report,
                              const char *fmt,
                              ...);
void yvex_graph_report_clear(yvex_graph_report *report);

int yvex_graph_report_build(const yvex_graph_report_request *request,
                            yvex_graph_report *report,
                            yvex_error *err);
int yvex_graph_plan_report_build(const yvex_graph_report_request *request,
                                 yvex_graph_report *report,
                                 yvex_error *err);
int yvex_graph_guard_report_build(const yvex_graph_report_request *request,
                                  yvex_graph_report *report,
                                  yvex_error *err);
int yvex_graph_primitive_report_build(const yvex_graph_report_request *request,
                                      yvex_graph_report *report,
                                      yvex_error *err);

#endif
