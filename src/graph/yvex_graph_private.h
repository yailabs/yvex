/*
 * yvex_graph_private.h - graph-domain private structures.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   private graph, memory-plan, and plan state shared by graph-domain files.
 *
 * Does not own:
 *   CLI input parsing, command dispatch, rendering, stdout/stderr output,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   private structures are shared only inside graph-domain implementation
 *   files; no CLI/operator headers are included here.
 *
 * Boundary:
 *   graph private state is not graph execution or generation readiness.
 */
#ifndef YVEX_GRAPH_PRIVATE_H
#define YVEX_GRAPH_PRIVATE_H

#include <stddef.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/engine.h>
#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/graph.h>
#include <yvex/memory_plan.h>
#include <yvex/model.h>
#include <yvex/op.h>
#include <yvex/planner.h>
#include <yvex/tensor.h>
#include <yvex/token_input.h>
#include <yvex/tokenizer.h>
#include <yvex/yvex.h>

typedef struct {
    unsigned int input_ids[4];
    unsigned int output_ids[4];
} yvex_graph_op_edges;

struct yvex_graph {
    yvex_graph_status status;
    char *architecture;
    char *model_name;
    unsigned long long sequence_length;
    unsigned long long context_length;
    yvex_graph_value_info *values;
    unsigned long long value_count;
    unsigned long long value_cap;
    yvex_graph_op_info *ops;
    yvex_graph_op_edges *edges;
    unsigned long long op_count;
    unsigned long long op_cap;
    yvex_graph_missing_required *missing;
    unsigned long long missing_count;
    unsigned long long missing_cap;
};

struct yvex_memory_plan {
    yvex_memory_plan_status status;
    yvex_memory_plan_summary summary;
};

struct yvex_plan {
    char *backend_name;
    char *backend_status;
    int backend_tensor_alloc;
    int backend_tensor_read_write;
    int backend_op_embed;
    int backend_op_matmul;
    int backend_op_mlp;
    int backend_op_rms_norm;
    int backend_op_rope;
    int backend_op_attention;
    yvex_graph *graph;
    yvex_memory_plan *memory;
};

typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
} yvex_cli_tokenizer_context;

typedef struct {
    const char *guard_status;
    const char *phase;
    const char *graph_kind;
    const char *integrity_status;
    const char *identity_status;
    const char *metadata_status;
    const char *shape_status;
    const char *range_status;
    const char *slice_range_status;
    const char *backend_status;
    const char *backend_op_status;
    int dispatch_attempted;
    int reference_read_attempted;
    int output_allocation_attempted;
    int cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned;
    unsigned long long output_bytes_allocated;
    unsigned long long reference_bytes_planned;
} yvex_cli_graph_guard_report;

typedef struct {
    const char *backend_name;
    unsigned long long layers;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    const float *initial_position_values;
    unsigned long long initial_position_value_count;
} yvex_cli_layer_fixture_options;

typedef struct {
    int executed;
    const char *status;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *backend_status;
    const char *backend_op_status;
    unsigned long long layers;
    unsigned long long total_op_count;
    unsigned long long output_bytes;
    unsigned long long scratch_bytes;
    unsigned long long final_output_checksum;
    unsigned long long final_reference_checksum;
    double final_max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
} yvex_cli_layer_fixture_result;

char *yvex_graph_strdup(const char *text);
void yvex_graph_value_clear(yvex_graph_value_info *value);
void yvex_graph_op_clear(yvex_graph_op_info *op);
void yvex_graph_missing_clear(yvex_graph_missing_required *missing);

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err);
int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err);
int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err);

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index);
int yvex_graph_backend_valid(const char *name);
yvex_backend_kind yvex_graph_backend_kind_from_name(const char *name);
int yvex_graph_exit_for_status(int status);

int open_model_context(const char *model_arg,
                       yvex_cli_tokenizer_context *ctx,
                       yvex_error *err);
void close_model_context(yvex_cli_tokenizer_context *ctx);
int enforce_registered_identity_cli(const yvex_model_ref *ref,
                                    const char *command_name);
int cli_token_input_vocab_from_model(const char *path,
                                     unsigned long long *out,
                                     yvex_error *err);
void print_token_input_summary(const yvex_token_input *input,
                               const char *status,
                               const char *bounds_status,
                               unsigned long long selected_index,
                               unsigned int selected_token,
                               int selected_seen);

void yvex_graph_guard_report_init(yvex_cli_graph_guard_report *report);
int preflight_graph_guard(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int requested_token,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);
int yvex_cli_graph_execute_layer_fixture(const yvex_cli_layer_fixture_options *options,
                                         yvex_cli_layer_fixture_result *out,
                                         yvex_error *err);

#endif
