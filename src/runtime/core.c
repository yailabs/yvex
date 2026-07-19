/* Owner: runtime engine and session lifecycle
 * Owns: runtime coordination, engine/session lifecycle state, runtime summaries, and typed session refusal
 *   boundaries not owned by narrower modules.
 * Does not own: graph kernels, tensor mapping, tokenizer metadata, benchmark harness, CLI rendering policy,
 *   artifact emission, or generation-loop behavior.
 * Invariants: runtime state is explicit and cleanup is owned; diagnostic runtime states must not be promoted into
 *   full model runtime claims; command handlers keep existing parser/exit-code behavior.
 * Boundary: diagnostic runtime is not full model runtime, generation support, eval evidence, benchmark evidence,
 *   throughput, or release readiness.
 * Purpose: bind admitted artifact/model/backend state into explicit engine and session lifecycles without
 *   performing model graph execution.
 * Inputs: typed engine/session options and admitted borrowed subsystem objects.
 * Effects: allocates and releases engine/session resources and mutates explicit lifecycle counters and diagnostic
 *   state.
 * Failure: returns typed state/admission/allocation failures after deterministic cleanup of partially constructed
 *   objects. */

#include <yvex/internal/runtime.h>
#include <yvex/internal/core.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/artifact.h>
#include <yvex/generation.h>
#include <yvex/runtime.h>

/* Runtime/session structs and small text/status helpers. */

struct yvex_session {
    const yvex_engine *engine;
    yvex_backend *backend;
    yvex_session_state state;
    unsigned long long context_length;
    unsigned long long max_tokens;
    unsigned long long position;
    unsigned long long accepted_tokens;
    unsigned long long rejected_tokens;
    int graph_partial;
    int backend_available;
    yvex_kv_cache *kv;
    yvex_logits *logits;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

/* Purpose: Implement the canonical set text reason mechanism owned by the runtime boundary. */
static void set_text_reason(char *out, size_t cap, const char *text)
{
    if (!out || cap == 0) {
        return;
    }
    snprintf(out, cap, "%s", text ? text : "");
}

/* Purpose: Implement the canonical set graph reason mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void set_graph_reason(char *out, size_t cap, const yvex_graph *graph)
{
    unsigned long long missing_count;
    int has_output_norm = 0;
    int has_output_head = 0;
    int has_token_embedding = 0;
    unsigned long long i;

    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';

    if (!graph) {
        snprintf(out, cap, "graph not built");
        return;
    }

    missing_count = yvex_graph_missing_required_count(graph);
    for (i = 0; i < missing_count; ++i) {
        const yvex_graph_missing_required *missing = yvex_graph_missing_required_at(graph, i);
        if (!missing) {
            continue;
        }
        if (missing->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING) {
            has_token_embedding = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_NORM) {
            has_output_norm = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD) {
            has_output_head = 1;
        }
    }

    if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        if (has_output_norm && has_output_head) {
            snprintf(out, cap, "graph partial; missing output_norm, output_head");
        } else if (has_output_norm) {
            snprintf(out, cap, "graph partial; missing output_norm");
        } else if (has_output_head) {
            snprintf(out, cap, "graph partial; missing output_head");
        } else if (missing_count > 0) {
            snprintf(out, cap, "graph partial; missing required tensor roles");
        } else {
            snprintf(out, cap, "graph partial");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_UNSUPPORTED) {
        if (has_token_embedding) {
            snprintf(out, cap, "graph unsupported; missing token_embedding");
        } else {
            snprintf(out, cap, "graph unsupported");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_BUILT) {
        snprintf(out, cap, "decode runtime not implemented in engine/session layer");
    } else {
        snprintf(out, cap, "graph status: %s", yvex_graph_status_name(yvex_graph_status_of(graph)));
    }
}

/* Engine lifecycle and backend attachment. */

/* Purpose: Implement the canonical set engine status from graph mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void set_engine_status_from_graph(yvex_engine *engine)
{
    if (!engine->graph) {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        set_text_reason(engine->reason, sizeof(engine->reason),
                                     "graph not requested; execution not implemented in engine/session layer");
        return;
    }

    if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        engine->status = YVEX_ENGINE_STATUS_PARTIAL;
        set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_UNSUPPORTED ||
               yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_INVALID) {
        engine->status = YVEX_ENGINE_STATUS_UNSUPPORTED;
        set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    }
}

/* Purpose: Implement the canonical attach engine weights mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int attach_engine_weights(yvex_engine *engine,
                                 const yvex_engine_options *options,
                                 yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_backend_kind kind;
    const char *backend_name;
    int rc;

    if (!engine || !options || !options->attach_weights) {
        return YVEX_OK;
    }

    backend_name = options->backend_name ? options->backend_name : "cpu";
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));

    rc = yvex_backend_kind_parse(backend_name, &kind, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    backend_options.kind = kind;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    materialize_options.backend_name = backend_name;
    materialize_options.require_all_tensors = options->require_all_weights;
    rc = yvex_weight_table_materialize(&weights,
                                       engine->artifact,
                                       engine->gguf,
                                       engine->tensors,
                                       backend,
                                       &materialize_options,
                                       err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        return rc;
    }

    rc = yvex_weight_table_get_summary(weights, &engine->weight_summary, err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        memset(&engine->weight_summary, 0, sizeof(engine->weight_summary));
        return rc;
    }

    engine->weight_backend = backend;
    engine->weights = weights;
    return YVEX_OK;
}

/* Purpose: open an engine descriptor over artifact, GGUF, tensor, model, tokenizer,
 * integrity, graph, and optional weight materialization state.
 * Inputs: options is borrowed; out receives owned engine state on success.
 * Effects: allocates engine state, opens artifact/GGUF views, builds metadata tables,
 * optionally builds a graph descriptor and materializes weights, and releases partial
 * ownership on failure.
 * Failure: returns invalid-arg, allocation, artifact, GGUF, integrity, graph, backend,
 * or materialization errors with cleanup before return.
 * Boundary: an opened engine descriptor is not full runtime execution, generation
 * support, eval evidence, benchmark evidence, throughput, or release readiness. */
int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err)
{
    yvex_engine *engine;
    yvex_artifact_options artifact_options;
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_graph_build_options graph_options;
    int rc;
    int load_tokenizer;
    int build_default_graph;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!options || !options->model_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "model_path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    engine = (yvex_engine *)calloc(1, sizeof(*engine));
    if (!engine) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to allocate engine");
        return YVEX_ERR_NOMEM;
    }
    engine->status = YVEX_ENGINE_STATUS_EMPTY;
    engine->model_path = yvex_core_strdup(options->model_path);
    if (!engine->model_path) {
        yvex_engine_close(engine);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to copy model path");
        return YVEX_ERR_NOMEM;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    artifact_options.path = options->model_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;

    rc = yvex_artifact_open(&engine->artifact, &artifact_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&engine->gguf, engine->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&engine->tensors, engine->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&engine->model, engine->gguf, engine->tensors, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(engine->artifact,
                                              engine->gguf,
                                              engine->tensors,
                                              &integrity_options,
                                              &integrity_report,
                                              err);
    }

    load_tokenizer = options->load_tokenizer != 0;
    if (rc == YVEX_OK && load_tokenizer) {
        rc = yvex_tokenizer_from_gguf(&engine->tokenizer, engine->gguf, engine->model, err);
    }

    build_default_graph = options->build_default_graph != 0;
    if (rc == YVEX_OK && build_default_graph) {
        memset(&graph_options, 0, sizeof(graph_options));
        graph_options.sequence_length = 1;
        graph_options.context_length = yvex_model_context_length(engine->model) > 0
                                           ? yvex_model_context_length(engine->model)
                                           : 1;
        graph_options.include_prefill_path = 1;
        rc = yvex_graph_build_for_model(&engine->graph, engine->model, engine->tensors, &graph_options, err);
    }
    if (rc == YVEX_OK) {
        rc = attach_engine_weights(engine, options, err);
    }

    if (rc != YVEX_OK) {
        engine->status = YVEX_ENGINE_STATUS_FAILED;
        yvex_engine_close(engine);
        return rc;
    }

    set_engine_status_from_graph(engine);
    *out = engine;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Construct the admitted engine open path state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err)
{
    yvex_engine_options options;

    memset(&options, 0, sizeof(options));
    options.model_path = model_path;
    options.load_tokenizer = 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    return yvex_engine_open(out, &options, err);
}

/* Purpose: Release the resources owned by engine close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
void yvex_engine_close(yvex_engine *engine)
{
    if (!engine) {
        return;
    }
    yvex_weight_table_close(engine->weights);
    yvex_backend_close(engine->weight_backend);
    yvex_graph_close(engine->graph);
    yvex_tokenizer_close(engine->tokenizer);
    yvex_model_descriptor_close(engine->model);
    yvex_tensor_table_close(engine->tensors);
    yvex_gguf_close(engine->gguf);
    yvex_artifact_close(engine->artifact);
    free(engine->model_path);
    free(engine);
}

/* Purpose: Implement the canonical engine status of mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
yvex_engine_status yvex_engine_status_of(const yvex_engine *engine)
{
    return engine ? engine->status : YVEX_ENGINE_STATUS_EMPTY;
}

/* Purpose: Return the canonical diagnostic label for engine status name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const char *yvex_engine_status_name(yvex_engine_status status)
{
    switch (status) {
    case YVEX_ENGINE_STATUS_EMPTY: return "empty";
    case YVEX_ENGINE_STATUS_LOADED: return "loaded";
    case YVEX_ENGINE_STATUS_PARTIAL: return "partial";
    case YVEX_ENGINE_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_ENGINE_STATUS_FAILED: return "failed";
    }
    return "unknown";
}
/* Purpose: Implement the canonical engine model path mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const char *yvex_engine_model_path(const yvex_engine *engine)
{
    return engine && engine->model_path ? engine->model_path : "";
}

/* Purpose: Implement the canonical engine model mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine)
{
    return engine ? engine->model : NULL;
}

/* Purpose: Implement the canonical engine tensors mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine)
{
    return engine ? engine->tensors : NULL;
}

/* Purpose: Implement the canonical engine tokenizer mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine)
{
    return engine ? engine->tokenizer : NULL;
}

/* Purpose: Implement the canonical engine graph mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const yvex_graph *yvex_engine_graph(const yvex_engine *engine)
{
    return engine ? engine->graph : NULL;
}

/* Purpose: copy engine descriptor and materialization summary facts for diagnostics.
 * Inputs: engine is borrowed; out receives a by-value summary.
 * Effects: mutates only out/err and may query weight materialization summary state;
 * it does not execute graph work or move tensor payload bytes.
 * Failure: returns invalid-arg for missing inputs and propagates summary failures from owned weight state.
 * Boundary: engine summary facts are not runtime readiness, generation support,
 * eval evidence, benchmark evidence, throughput, or release readiness. */
int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err)
{
    const yvex_gguf_header *header;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_get_summary", "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->status = engine->status;
    out->model_path = yvex_engine_model_path(engine);
    out->model_name = yvex_model_name(engine->model);
    out->architecture = yvex_arch_name(yvex_model_arch(engine->model));
    if (engine->gguf) {
        header = yvex_gguf_header_view(engine->gguf);
        if (header) {
            out->metadata_count = header->metadata_count;
            out->tensor_count = header->tensor_count;
        }
    }
    out->known_tensor_bytes = yvex_model_total_storage_bytes(engine->model);
    out->unsupported_tensor_accounting =
        yvex_model_unsupported_tensor_accounting_count(engine->model);
    out->tokenizer_model = engine->tokenizer
                               ? yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(engine->tokenizer))
                               : "absent";
    out->tokenizer_support = engine->tokenizer
                                 ? yvex_tokenizer_support_name(yvex_tokenizer_support_of(engine->tokenizer))
                                 : "absent";
    out->graph_status = engine->graph
                            ? yvex_graph_status_name(yvex_graph_status_of(engine->graph))
                            : "not-built";
    out->weights_attached = engine->weights != NULL;
    out->weights_backend = engine->weight_summary.backend_name
                               ? engine->weight_summary.backend_name
                               : "none";
    out->weight_tensor_count = engine->weight_summary.tensors_materialized;
    out->weight_total_bytes = engine->weight_summary.bytes_materialized;
    out->weight_backend_allocated_bytes = engine->weight_summary.backend_allocated_bytes;
    out->graph_execution_ready = 0;

    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Implement the canonical engine diagnostic reason mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const char *yvex_engine_diagnostic_reason(const yvex_engine *engine)
{
    return engine ? engine->reason : "";
}

/* Session lifecycle and session state. */

/* Purpose: Implement the canonical set session reason from graph mechanism owned by the runtime boundary. */
static void set_session_reason_from_graph(yvex_session *session)
{
    const yvex_graph *graph = yvex_engine_graph(session->engine);

    set_graph_reason(session->reason, sizeof(session->reason), graph);
}

/* Purpose: Implement the canonical reset state for graph mechanism owned by the runtime boundary. */
static yvex_session_state reset_state_for_graph(const yvex_session *session)
{
    return session->graph_partial ? YVEX_SESSION_STATE_PARTIAL : YVEX_SESSION_STATE_READY;
}

/* Purpose: admit a session mutation only while its owned lifecycle is open.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int session_require_open(const yvex_session *session,
                                const char *where,
                                yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "session is closed");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

/* Purpose: allocate diagnostic session state around an existing engine and backend.
 * Inputs: engine/backend/options are borrowed; out receives owned session state.
 * Effects: allocates KV/logits diagnostic buffers where requested, initializes counters
 * and graph status, and cleans up partial allocations on failure.
 * Failure: returns invalid-arg, allocation, KV/logits, or backend-related errors with cleanup before returning.
 * Boundary: session state is diagnostic runtime state and not full model runtime,
 * generation support, eval evidence, benchmark evidence, or release readiness. */
int yvex_session_create(yvex_session **out,
                        const yvex_engine *engine,
                        yvex_backend *backend,
                        const yvex_session_options *options,
                        yvex_error *err)
{
    yvex_session *session;
    unsigned long long context_length;
    int allow_partial = 1;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!engine || !backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_create", "engine and backend are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ||
        !yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_create",
                       "backend lacks exact tensor allocation/transfer capabilities");
        return YVEX_ERR_UNSUPPORTED;
    }

    context_length = yvex_model_context_length(yvex_engine_model(engine));
    if (options && options->context_length > 0) {
        context_length = options->context_length;
    }
    if (context_length == 0) {
        context_length = 1;
    }
    if (options) {
        allow_partial = options->allow_partial_graph;
    }

    session = (yvex_session *)calloc(1, sizeof(*session));
    if (!session) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_session_create", "failed to allocate session");
        return YVEX_ERR_NOMEM;
    }

    session->engine = engine;
    session->backend = backend;
    session->state = YVEX_SESSION_STATE_CREATED;
    session->context_length = context_length;
    session->max_tokens = options ? options->max_tokens : 0;
    session->backend_available = 1;
    session->graph_partial = yvex_engine_status_of(engine) == YVEX_ENGINE_STATUS_PARTIAL ||
                             (yvex_engine_graph(engine) &&
                              yvex_graph_status_of(yvex_engine_graph(engine)) == YVEX_GRAPH_STATUS_PARTIAL);

    if (session->graph_partial && !allow_partial) {
        yvex_session_close(session);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_create",
                       "graph is partial; allow_partial_graph is required in engine/session layer");
        return YVEX_ERR_UNSUPPORTED;
    }

    if (options && options->create_kv) {
        rc = yvex_kv_cache_create_shape(&session->kv, &options->kv_shape, err);
    } else {
        rc = yvex_kv_cache_create(&session->kv, yvex_engine_model(engine), context_length, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_logits_create(&session->logits, yvex_engine_model(engine), err);
    }
    if (rc != YVEX_OK) {
        yvex_session_close(session);
        return rc;
    }

    set_session_reason_from_graph(session);
    session->state = reset_state_for_graph(session);
    *out = session;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Release the resources owned by session close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
void yvex_session_close(yvex_session *session)
{
    if (!session) {
        return;
    }
    session->state = YVEX_SESSION_STATE_CLOSED;
    yvex_logits_close(session->logits);
    yvex_kv_cache_close(session->kv);
    free(session);
}

/* Purpose: Implement the canonical session state of mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
yvex_session_state yvex_session_state_of(const yvex_session *session)
{
    return session ? session->state : YVEX_SESSION_STATE_CLOSED;
}

/* Purpose: Implement the canonical session position mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
unsigned long long yvex_session_position(const yvex_session *session)
{
    return session ? session->position : 0;
}

/* Purpose: Implement the canonical session context length mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
unsigned long long yvex_session_context_length(const yvex_session *session)
{
    return session ? session->context_length : 0;
}

/* Purpose: copy diagnostic session, KV, logits, and engine facts into a summary.
 * Inputs: session is borrowed; out receives by-value report facts.
 * Effects: mutates only out/err and reads summaries from owned subobjects; it does not
 * advance decode state or execute graph work.
 * Failure: returns invalid-arg for missing inputs and otherwise preserves best-effort
 * summary fields when optional sub-summaries are unavailable.
 * Boundary: session summaries are not runtime generation, eval evidence, benchmark
 * evidence, throughput, or release readiness. */
int yvex_session_get_summary(const yvex_session *session,
                             yvex_session_summary *out,
                             yvex_error *err)
{
    yvex_kv_summary kv_summary;
    yvex_logits_summary logits_summary;
    yvex_engine_summary engine_summary;

    if (!session || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_get_summary", "session and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->state = session->state;
    out->engine_status = yvex_engine_status_name(yvex_engine_status_of(session->engine));
    out->backend_kind = yvex_backend_kind_name(yvex_backend_kind_of(session->backend));
    out->backend_status = yvex_backend_status_name(yvex_backend_status_of(session->backend));
    out->context_length = session->context_length;
    out->position = session->position;
    out->accepted_tokens = session->accepted_tokens;
    out->rejected_tokens = session->rejected_tokens;
    out->graph_partial = session->graph_partial;
    out->backend_available = session->backend_available;
    out->execution_ready = 0;
    out->graph_execution_ready = 0;

    if (yvex_engine_get_summary(session->engine, &engine_summary, err) == YVEX_OK) {
        out->weights_attached = engine_summary.weights_attached;
        out->weights_backend = engine_summary.weights_backend;
        out->weight_tensor_count = engine_summary.weight_tensor_count;
        out->weight_total_bytes = engine_summary.weight_total_bytes;
    }

    if (session->kv && yvex_kv_cache_get_summary(session->kv, &kv_summary, err) == YVEX_OK) {
        out->kv_status = yvex_kv_status_name(kv_summary.status);
        out->kv_bytes = kv_summary.bytes;
        out->kv_owner = kv_summary.owner;
        out->kv_dtype = kv_summary.dtype;
        out->kv_layers = kv_summary.layer_count;
        out->kv_heads = kv_summary.kv_head_count;
        out->kv_head_dim = kv_summary.head_dim;
        out->kv_capacity = kv_summary.context_length;
        out->kv_bytes_per_position = kv_summary.bytes_per_position;
        out->kv_allocated_bytes = kv_summary.allocated_bytes;
        out->kv_written_positions = kv_summary.written_positions;
        out->kv_append_count = kv_summary.append_count;
        out->kv_read_count = kv_summary.read_count;
        out->kv_overflow_status = kv_summary.overflow_status;
        out->kv_cleanup_status = kv_summary.cleanup_status;
        out->kv_session_owned = kv_summary.session_owned;
    } else {
        out->kv_status = "empty";
        out->kv_owner = "none";
        out->kv_dtype = "none";
        out->kv_overflow_status = "not-checked";
        out->kv_cleanup_status = "not-needed";
    }
    if (session->logits && yvex_logits_get_summary(session->logits, &logits_summary, err) == YVEX_OK) {
        out->logits_status = yvex_logits_status_name(logits_summary.status);
        out->logits_capacity = logits_summary.vocab_size;
    } else {
        out->logits_status = "empty";
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Implement the canonical session diagnostic reason mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const char *yvex_session_diagnostic_reason(const yvex_session *session)
{
    return session ? session->reason : "";
}

/* Session KV wrappers and unsupported decode/prefill boundaries. */

/* Purpose: Implement the canonical session accept tokens mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_accept_tokens(yvex_session *session,
                               const yvex_tokens *tokens,
                               yvex_error *err)
{
    int status;

    if (!tokens) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_accept_tokens",
                       "session and tokens are required");
        return YVEX_ERR_INVALID_ARG;
    }
    status = session_require_open(session, "yvex_session_accept_tokens", err);
    if (status != YVEX_OK) {
        return status;
    }
    if (tokens->len > session->context_length ||
        session->position > session->context_length - tokens->len) {
        session->rejected_tokens += tokens->len;
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_session_accept_tokens",
                        "accepting %llu tokens would exceed context length %llu",
                        tokens->len, session->context_length);
        return YVEX_ERR_BOUNDS;
    }

    session->position += tokens->len;
    session->accepted_tokens += tokens->len;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Advance session prefill across the admitted prompt while preserving session and trace transactions.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_prefill(yvex_session *session,
                         const yvex_tokens *tokens,
                         yvex_error *err)
{
    (void)tokens;

    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_prefill", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->graph_partial) {
        session->state = YVEX_SESSION_STATE_PARTIAL;
        set_session_reason_from_graph(session);
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_session_prefill",
                        "prefill is not executable in engine/session layer because %s", session->reason);
        return YVEX_ERR_UNSUPPORTED;
    }

    set_text_reason(session->reason, sizeof(session->reason),
                                 "prefill runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_prefill",
                   "prefill runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

/* Purpose: Decode session decode next according to its pinned numeric representation.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_decode_next(yvex_session *session,
                             unsigned int *out_token,
                             yvex_error *err)
{
    if (!session || !out_token) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_decode_next",
                       "session and out_token are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out_token = 0;
    set_text_reason(session->reason, sizeof(session->reason),
                                 "decode runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_decode_next",
                   "decode runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

/* Purpose: Implement the canonical session cancel mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_cancel(yvex_session *session,
                        yvex_error *err)
{
    int status = session_require_open(session, "yvex_session_cancel", err);

    if (status != YVEX_OK) {
        return status;
    }
    session->state = YVEX_SESSION_STATE_CANCELLED;
    set_text_reason(session->reason, sizeof(session->reason), "session cancelled");
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Implement the canonical session reset mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_reset(yvex_session *session,
                       yvex_error *err)
{
    int status = session_require_open(session, "yvex_session_reset", err);

    if (status != YVEX_OK) {
        return status;
    }

    session->position = 0;
    session->accepted_tokens = 0;
    session->rejected_tokens = 0;
    (void)yvex_kv_cache_clear(session->kv, err);
    session->state = reset_state_for_graph(session);
    set_session_reason_from_graph(session);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Append session kv append position F32 while preserving checked capacity and deterministic order.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_kv_append_position_f32(yvex_session *session,
                                        const float *values,
                                        unsigned long long value_count,
                                        unsigned long long *out_position,
                                        yvex_error *err)
{
    int status = session_require_open(session, "yvex_session_kv_append_position_f32", err);

    if (status != YVEX_OK) {
        return status;
    }
    return yvex_kv_cache_append_position_f32(session->kv, values, value_count, out_position, err);
}

/* Purpose: Retrieve session kv read position F32 from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_kv_read_position_f32(yvex_session *session,
                                      unsigned long long position,
                                      float *out_values,
                                      unsigned long long value_count,
                                      yvex_error *err)
{
    int status = session_require_open(session, "yvex_session_kv_read_position_f32", err);

    if (status != YVEX_OK) {
        return status;
    }
    return yvex_kv_cache_read_position_f32(session->kv, position, out_values, value_count, err);
}

/* Purpose: Implement the canonical session kv clear mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_session_kv_clear(yvex_session *session,
                          yvex_error *err)
{
    int status = session_require_open(session, "yvex_session_kv_clear", err);

    if (status != YVEX_OK) {
        return status;
    }
    return yvex_kv_cache_clear(session->kv, err);
}

/* Purpose: Return the canonical diagnostic label for session state name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
const char *yvex_session_state_name(yvex_session_state state)
{
    switch (state) {
    case YVEX_SESSION_STATE_CREATED: return "created";
    case YVEX_SESSION_STATE_READY: return "ready";
    case YVEX_SESSION_STATE_PREFILLING: return "prefilling";
    case YVEX_SESSION_STATE_DECODING: return "decoding";
    case YVEX_SESSION_STATE_PARTIAL: return "partial";
    case YVEX_SESSION_STATE_CANCELLED: return "cancelled";
    case YVEX_SESSION_STATE_FAILED: return "failed";
    case YVEX_SESSION_STATE_CLOSED: return "closed";
    }
    return "unknown";
}
