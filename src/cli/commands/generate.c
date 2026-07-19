/* Owner: src/cli/commands.
 * Owns: generate command dispatch from typed input to generation report and renderer.
 * Does not own: generation report facts, runtime state, trace rendering internals, eval, or benchmark.
 * Invariants: adapter stays thin and does not hide domain behavior.
 * Boundary: command dispatch is not full-model generation.
 * Purpose: bind generate CLI input to the typed diagnostic generation API.
 * Inputs: argv from yvex generate.
 * Effects: renders help, parser errors, or diagnostic generation reports.
 * Failure: returns parser, runtime, or renderer exit codes. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *const literal_lines_0[] = { "full_transformer_prefill_ready: false",
    "decode_ready: false",
    "logits_ready: false",
    "generation_ready: false",
    "generation: unsupported"};

#define SUMMARY_FIELD(K, F, T, D) \
    {K, T, offsetof(yvex_prefill_state_summary, F), D}

static const yvex_render_field_spec prefill_identity_fields[] = {
    SUMMARY_FIELD("prefill_state_created", prefill_state_created,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("prefill_state_kind", prefill_state_kind,
                  YVEX_RENDER_FIELD_TEXT, "segment-summary"),
    SUMMARY_FIELD("sequence_execution_mode", sequence_execution_mode,
                  YVEX_RENDER_FIELD_TEXT, "independent-token-segments"),
    SUMMARY_FIELD("prefill_phase", prefill_phase,
                  YVEX_RENDER_FIELD_TEXT, "preflight")
};

static const yvex_render_field_spec prefill_execution_fields[] = {
    SUMMARY_FIELD("token_count", token_count, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("tokens_processed", tokens_processed,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("position_start", position_start, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("position_end", position_end, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("failed_token_index", failed_token_index,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("context_boundary_status", context_boundary_status,
                  YVEX_RENDER_FIELD_TEXT, "unchecked"),
    SUMMARY_FIELD("context_length", context_length, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("chunked_prefill_requested", chunked_prefill_requested,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("chunk_execution_mode", chunk_execution_mode,
                  YVEX_RENDER_FIELD_TEXT, "token-loop"),
    SUMMARY_FIELD("chunk_size", chunk_size, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("chunk_count", chunk_count, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("chunks_processed", chunks_processed, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("failed_chunk_index", failed_chunk_index,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("current_chunk_start", current_chunk_start,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("current_chunk_end", current_chunk_end,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("final_chunk_checksum", final_chunk_checksum,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_scratch_kind", prefill_scratch_kind,
                  YVEX_RENDER_FIELD_TEXT, "host-diagnostic-reuse"),
    SUMMARY_FIELD("prefill_scratch_reuse", prefill_scratch_reuse,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("prefill_scratch_allocations", prefill_scratch_allocations,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_scratch_reuse_count", prefill_scratch_reuse_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_scratch_cleanup_attempted",
                  prefill_scratch_cleanup_attempted, YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("prefill_scratch_cleanup_status", prefill_scratch_cleanup_status,
                  YVEX_RENDER_FIELD_TEXT, "not-needed"),
    SUMMARY_FIELD("segment_graph_executions", segment_graph_executions,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("segment_output_count", segment_output_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("segment_output_bytes", segment_output_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_aggregate_checksum", aggregate_checksum,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_final_token_checksum", final_token_checksum,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_total_output_bytes", total_output_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_scratch_bytes", scratch_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("prefill_max_abs_diff", max_abs_diff,
                  YVEX_RENDER_FIELD_DOUBLE, NULL),
    SUMMARY_FIELD("layer_prefill_requested", layer_prefill_requested,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("layer_execution_kind", layer_execution_kind,
                  YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("model_layer_execution", model_layer_execution,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("layer_input_projection", layer_input_projection,
                  YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("layer_handoff", layer_handoff,
                  YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("layer_sequence_rebuild", layer_sequence_rebuild,
                  YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("layer_count", layer_count, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_graph_executions", layer_graph_executions,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_block_executions", layer_block_executions,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_total_op_count", layer_total_op_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_output_count", layer_output_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_output_bytes", layer_output_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_total_output_bytes", layer_total_output_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_total_scratch_bytes", layer_total_scratch_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_final_checksum", layer_final_checksum,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_final_reference_checksum", layer_final_reference_checksum,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("layer_max_abs_diff", layer_max_abs_diff,
                  YVEX_RENDER_FIELD_DOUBLE, NULL)
};

static const yvex_render_field_spec prefill_cleanup_fields[] = {
    SUMMARY_FIELD("cleanup_attempted", cleanup_attempted,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("cleanup_status", cleanup_status,
                  YVEX_RENDER_FIELD_TEXT, "not-needed")
};

static const yvex_render_field_spec prefill_kv_fields[] = {
    SUMMARY_FIELD("kv_ready", kv_ready, YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("session_kv_owned", session_kv_owned,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("kv_bound_to_prefill", kv_bound_to_prefill,
                  YVEX_RENDER_FIELD_BOOL, NULL),
    SUMMARY_FIELD("kv_binding_kind", kv_binding_kind,
                  YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("kv_binding_source", kv_binding_source,
                  YVEX_RENDER_FIELD_TEXT, "segment-output-sample"),
    SUMMARY_FIELD("kv_status", kv_status, YVEX_RENDER_FIELD_TEXT, "not-requested"),
    SUMMARY_FIELD("kv_owner", kv_owner, YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("kv_dtype", kv_dtype, YVEX_RENDER_FIELD_TEXT, "none"),
    SUMMARY_FIELD("kv_layers", kv_layers, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_heads", kv_heads, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_head_dim", kv_head_dim, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_capacity", kv_capacity, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_values_per_position", kv_values_per_position,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_bytes_per_position", kv_bytes_per_position,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_planned_bytes", kv_planned_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_allocated_bytes", kv_allocated_bytes,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_positions_written", kv_positions_written,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_append_count", kv_append_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_read_count", kv_read_count, YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_read_position", kv_read_position,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_read_value_count", kv_read_value_count,
                  YVEX_RENDER_FIELD_U64, NULL),
    SUMMARY_FIELD("kv_read_checksum", kv_read_checksum,
                  YVEX_RENDER_FIELD_U64, NULL)
};

static const yvex_render_field_spec prefill_kv_tail_fields[] = {
    SUMMARY_FIELD("kv_overflow", kv_overflow_status,
                  YVEX_RENDER_FIELD_TEXT, "not-checked"),
    SUMMARY_FIELD("kv_cleanup_status", kv_cleanup_status,
                  YVEX_RENDER_FIELD_TEXT, "not-needed")
};

#undef SUMMARY_FIELD

/* Purpose: Render print prefill state summary from typed facts (`print_prefill_state_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void print_prefill_state_summary(const yvex_prefill_state_summary *summary,
                                        const char *model_arg,
                                        const char *backend_name,
                                        const char *token_input_status,
                                        const char *status)
{
    const yvex_prefill_state_summary empty = {0};
    const yvex_prefill_state_summary *facts = summary ? summary : &empty;
    unsigned long long i;

    yvex_cli_out_line(stdout, "prefill: state");
    render_object_fields(stdout, facts, prefill_identity_fields,
                         sizeof(prefill_identity_fields) / sizeof(prefill_identity_fields[0]));
    yvex_cli_out_kv_str(stdout, "model", model_arg ? model_arg : "");
    yvex_cli_out_kv_str(stdout, "backend",
           facts->backend_name && strcmp(facts->backend_name, "none") != 0
               ? facts->backend_name
               : (backend_name ? backend_name : "cpu"));
    yvex_cli_out_kv_str(stdout, "segment",
                        facts->segment_name ? facts->segment_name : "embedding-rmsnorm");
    yvex_cli_out_kv_str(stdout, "token_input_status",
                        token_input_status ? token_input_status : "fail");
    render_object_fields(stdout, facts, prefill_execution_fields,
                         sizeof(prefill_execution_fields) / sizeof(prefill_execution_fields[0]));
    yvex_cli_out_writef(stdout, "layer_output_sample_values:");
    if (summary && summary->layer_output_sample_count > 0ull) {
        for (i = 0; i < summary->layer_output_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? "" : ",",
                   (double)summary->layer_output_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "\n");
    render_object_fields(stdout, facts, prefill_cleanup_fields,
                         sizeof(prefill_cleanup_fields) / sizeof(prefill_cleanup_fields[0]));
    if (summary && summary->cuda_parity) {
        yvex_cli_out_line(stdout, "prefill_cuda_parity: pass");
    }
    render_object_fields(stdout, facts, prefill_kv_fields,
                         sizeof(prefill_kv_fields) / sizeof(prefill_kv_fields[0]));
    yvex_cli_out_writef(stdout, "kv_read_sample_values:");
    if (summary && summary->kv_read_sample_count > 0ull) {
        for (i = 0; i < summary->kv_read_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? "" : ",",
                (double)summary->kv_read_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "\n");
    render_object_fields(stdout, facts, prefill_kv_tail_fields,
                         sizeof(prefill_kv_tail_fields) / sizeof(prefill_kv_tail_fields[0]));
    yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "prefill-state-fail");
}

/* Segment-summary prefill command surface. */

/* Purpose: Construct the owned init prefill summary defaults state (`init_prefill_summary_cli_defaults`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void init_prefill_summary_cli_defaults(yvex_prefill_state_summary *summary,
                                              const char *segment_name,
                                              int attach_kv,
                                              const yvex_kv_shape *shape,
                                              unsigned long long layer_count,
                                              unsigned long long chunk_size,
                                              unsigned long long position_start,
                                              unsigned long long context_length)
{
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->prefill_state_kind = layer_count > 0ull
                                      ? "layer-backed-segment-summary"
                                      : "segment-summary";
    summary->sequence_execution_mode = layer_count > 0ull
                                           ? "segment-then-controlled-layer-fixture"
                                           : "independent-token-segments";
    summary->prefill_phase = "preflight";
    summary->segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    summary->position_start = position_start;
    summary->position_end = position_start;
    summary->chunked_prefill_requested = chunk_size > 0ull;
    summary->chunk_execution_mode = chunk_size > 0ull ? "bounded-token-chunks" : "token-loop";
    summary->chunk_size = chunk_size;
    summary->context_length = context_length;
    summary->context_boundary_status = "unchecked";
    summary->prefill_scratch_kind = "host-diagnostic-reuse";
    summary->prefill_scratch_reuse = chunk_size > 0ull;
    summary->prefill_scratch_cleanup_status = "not-needed";
    summary->cleanup_status = "not-needed";
    summary->generation_status = "unsupported";
    summary->layer_prefill_requested = layer_count > 0ull;
    summary->layer_execution_kind = layer_count > 0ull ? "controlled-layer-fixture" : "none";
    summary->layer_input_projection = layer_count > 0ull ? "segment-sample-prefix" : "none";
    summary->layer_handoff = layer_count > 0ull ? "selected-position-row" : "none";
    summary->layer_sequence_rebuild = layer_count > 0ull
                                          ? "deterministic-with-previous-position-row"
                                          : "none";
    summary->layer_count = layer_count;
    summary->kv_binding_kind = attach_kv ? "minimal-diagnostic" : "none";
    summary->kv_binding_source = layer_count > 0ull
                                     ? "layer-final-sample"
                                     : "segment-output-sample";
    summary->kv_status = attach_kv ? "planned" : "not-requested";
    summary->kv_owner = "none";
    summary->kv_dtype = "none";
    summary->kv_overflow_status = "not-checked";
    summary->kv_cleanup_status = "not-needed";
    if (shape) {
        summary->kv_layers = shape->layer_count;
        summary->kv_heads = shape->kv_head_count;
        summary->kv_head_dim = shape->head_dim;
        summary->kv_capacity = shape->capacity;
    }
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
    int attach_kv;
    int kv_shape_seen;
    int layer_count_seen;
    int layer_hidden_seen;
    int layer_head_seen;
    int layer_ffn_seen;
    int chunk_size_seen;
} prefill_cli_options;

/* Admit one named backend through the canonical backend registry. */
/* Parse one prefill shape or position option and retain its admission bit. */
/* Purpose: Parse prefill parse number into typed CLI state (`prefill_parse_number`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int prefill_parse_number(int arg_count,
                                char **args,
                                int *index,
                                prefill_cli_options *options)
{
    const char *name = args[*index];
    const char *error = NULL;
    unsigned long long *value = NULL;
    int *seen = NULL;
    int allow_zero = 0;

    if (strcmp(name, "--kv-layers") == 0) {
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
                     : parse_positive_ull(args[*index + 1], value))) {
        yvex_cli_out_writef(stderr, "%s", error);
        return -1;
    }
    if (seen) *seen = 1;
    *index += 1;
    return 1;
}

/* Parse prefill command options without opening model state. */
/* Purpose: Parse prefill options parse into typed CLI state (`prefill_options_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int prefill_options_parse(int arg_count, char **args, prefill_cli_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    for (i = 2; i < arg_count; ++i) {
        int numeric = prefill_parse_number(arg_count, args, &i, options);
        if (numeric < 0) return 2;
        if (numeric > 0) continue;
        if (strcmp(args[i], "--attach-kv") == 0) {
            options->attach_kv = 1;
        } else if (strcmp(args[i], "--model") == 0 ||
                   strcmp(args[i], "--backend") == 0 ||
                   strcmp(args[i], "--segment") == 0 || strcmp(args[i], "--tokens") == 0) {
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
        } else if (!options->model) {
            options->model = args[i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown prefill option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help prefill' for usage.\n");
            return 2;
        }
    }
    return 0;
}

/* Validate prefill cross-option constraints and install default layer geometry. */
/* Purpose: Validate prefill options validate before downstream use (`prefill_options_validate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int prefill_options_validate(prefill_cli_options *options)
{
    if (!options->model || !options->tokens || !options->segment) {
        yvex_cli_out_writef(stderr,
            "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --"
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
        yvex_cli_out_writef(stderr, "yvex: unsupported prefill segment: %s\n", options->segment);
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

/* Purpose: Orchestrate the typed command prefill request (`command_prefill`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_prefill(int arg_count, char **args)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    prefill_cli_options cli;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_prefill_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    rc = prefill_options_parse(arg_count, args, &cli);
    if (rc != 0) return rc;
    rc = prefill_options_validate(&cli);
    if (rc != 0) return rc;

    rc = yvex_model_ref_resolve(&model_ref, cli.model, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "prefill");
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, cli.segment,
                                          cli.attach_kv, &cli.kv_shape, cli.layer_count,
                                          cli.chunk_size, cli.position_start,
                                          cli.context_length);
        print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                    "fail", "prefill-state-fail");
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
        init_prefill_summary_cli_defaults(&prefill_summary, cli.segment,
                                          cli.attach_kv, &cli.kv_shape, cli.layer_count,
                                          cli.chunk_size, cli.position_start,
                                          cli.context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                    "fail", "prefill-state-fail");
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
        init_prefill_summary_cli_defaults(&prefill_summary, cli.segment,
                                          cli.attach_kv, &cli.kv_shape, cli.layer_count,
                                          cli.chunk_size, cli.position_start,
                                          cli.context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                    "pass", "prefill-state-fail");
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
        init_prefill_summary_cli_defaults(&prefill_summary, cli.segment,
                                          cli.attach_kv, &cli.kv_shape, cli.layer_count,
                                          cli.chunk_size, cli.position_start,
                                          cli.context_length);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                    "pass", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    prefill_options.token_input = &token_input;
    prefill_options.segment_name = cli.segment;
    prefill_options.position_start = cli.position_start;
    prefill_options.chunk_size = cli.chunk_size_seen ? cli.chunk_size : 0ull;
    prefill_options.context_length = cli.context_length;
    prefill_options.attach_kv = cli.attach_kv;
    prefill_options.kv_shape = cli.kv_shape;
    prefill_options.layer_count = cli.layer_count_seen ? cli.layer_count : 0ull;
    prefill_options.layer_hidden_dim = cli.layer_hidden_dim;
    prefill_options.layer_head_dim = cli.layer_head_dim;
    prefill_options.layer_ffn_dim = cli.layer_ffn_dim;
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, &err);
    if (rc != YVEX_OK) {
        print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                    "pass", "prefill-state-fail");
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    print_prefill_state_summary(&prefill_summary, cli.model, cli.backend,
                                "pass", "prefill-state-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

/* Purpose: Parse generate print parse error into typed CLI state (`generate_cli_print_parse_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int generate_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

/* Purpose: Render generate print runtime error from typed facts (`generate_cli_print_runtime_error`). */
static int generate_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return exit_for_status(status);
}

/* Purpose: Orchestrate the typed generate command request (`yvex_generate_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_command(int argc, char **argv)
{
    yvex_generate_args args;
    yvex_generation_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_generate_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return generate_cli_print_parse_error(&err);
    }
    if (args.help_requested) {
        return yvex_generate_render_help(yvex_cli_out_stdout());
    }

    rc = yvex_generation_run_diagnostic(&args.request, &report, &err);
    if (report.loop_created) {
        (void)yvex_generate_render(yvex_cli_out_stdout(),
                                   args.render_mode,
                                   &report);
    }
    if (rc != YVEX_OK) {
        return generate_cli_print_runtime_error(&err, rc);
    }
    return 0;
}

/* Purpose: Orchestrate the typed prefill command request (`yvex_prefill_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_prefill_command(int arg_count, char **args)
{
    return command_prefill(arg_count, args);
}

/* Purpose: Render generate help from typed facts (`yvex_generate_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_generate_help(FILE *fp)
{
    (void)yvex_generate_render_help(fp);
}

/* Purpose: Render prefill help from typed facts (`yvex_prefill_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_prefill_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens "
            "IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--"
            "position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --"
            "kv-capacity N]\n\nPrefill records a segment-summary prefill foundation from validated token input "
            "and can bind processed positions to minimal KV ownership. Layer-backed prefill uses the selected "
            "embedding+RMSNorm segment plus a controlled layer fixture scheduler over a sampled row. Chunked "
            "prefill partitions validated token input into bounded diagnostic chunks with explicit scratch and "
            "context-boundary reporting. It is not full transformer prefill, decode, logits, sampling, or "
            "generation.\n");
}
