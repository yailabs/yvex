/*
 * yvex_runtime.c - Engine, session, KV, and logits shells.
 *
 * This file owns descriptor-only runtime state. It does not implement prefill,
 * decode, sampling, or generation.
 */

#include <stddef.h>
#include <yvex/yvex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#define YVEX_RUNTIME_REASON_CAP 256u

struct yvex_engine {
    char *model_path;
    yvex_engine_status status;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_graph *graph;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

struct yvex_kv_cache {
    yvex_kv_summary summary;
};

struct yvex_logits {
    yvex_logits_summary summary;
};

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

char *yvex_runtime_strdup(const char *text);
void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph);
void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text);



static void set_engine_status_from_graph(yvex_engine *engine)
{
    if (!engine->graph) {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_text_reason(engine->reason, sizeof(engine->reason),
                                     "graph not requested; execution not implemented in engine/session layer");
        return;
    }

    if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        engine->status = YVEX_ENGINE_STATUS_PARTIAL;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_UNSUPPORTED ||
               yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_INVALID) {
        engine->status = YVEX_ENGINE_STATUS_UNSUPPORTED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    }
}

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err)
{
    yvex_engine *engine;
    yvex_artifact_options artifact_options;
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
    engine->model_path = yvex_runtime_strdup(options->model_path);
    if (!engine->model_path) {
        yvex_engine_close(engine);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to copy model path");
        return YVEX_ERR_NOMEM;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
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

void yvex_engine_close(yvex_engine *engine)
{
    if (!engine) {
        return;
    }
    yvex_graph_close(engine->graph);
    yvex_tokenizer_close(engine->tokenizer);
    yvex_model_descriptor_close(engine->model);
    yvex_tensor_table_close(engine->tensors);
    yvex_gguf_close(engine->gguf);
    yvex_artifact_close(engine->artifact);
    free(engine->model_path);
    free(engine);
}

yvex_engine_status yvex_engine_status_of(const yvex_engine *engine)
{
    return engine ? engine->status : YVEX_ENGINE_STATUS_EMPTY;
}

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

const char *yvex_engine_model_path(const yvex_engine *engine)
{
    return engine && engine->model_path ? engine->model_path : "";
}

const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine)
{
    return engine ? engine->model : NULL;
}

const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine)
{
    return engine ? engine->tensors : NULL;
}

const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine)
{
    return engine ? engine->tokenizer : NULL;
}

const yvex_graph *yvex_engine_graph(const yvex_engine *engine)
{
    return engine ? engine->graph : NULL;
}

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

    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_engine_diagnostic_reason(const yvex_engine *engine)
{
    return engine ? engine->reason : "";
}



int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err)
{
    yvex_kv_cache *kv;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (context_length == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "context_length must be positive");
        return YVEX_ERR_INVALID_ARG;
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_kv_cache_create", "failed to allocate KV cache summary");
        return YVEX_ERR_NOMEM;
    }

    (void)model;
    kv->summary.status = YVEX_KV_STATUS_UNAVAILABLE;
    kv->summary.context_length = context_length;
    kv->summary.bytes = 0;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_kv_cache_close(yvex_kv_cache *kv)
{
    free(kv);
}

yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.status : YVEX_KV_STATUS_EMPTY;
}

const char *yvex_kv_status_name(yvex_kv_status status)
{
    switch (status) {
    case YVEX_KV_STATUS_EMPTY: return "empty";
    case YVEX_KV_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_KV_STATUS_PLANNED: return "planned";
    case YVEX_KV_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err)
{
    if (!kv || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_get_summary", "kv and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &kv->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}



int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err)
{
    yvex_logits *logits;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }

    logits = (yvex_logits *)calloc(1, sizeof(*logits));
    if (!logits) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_logits_create", "failed to allocate logits summary");
        return YVEX_ERR_NOMEM;
    }

    logits->summary.status = YVEX_LOGITS_STATUS_UNAVAILABLE;
    logits->summary.vocab_size = 0;
    logits->summary.bytes = 0;

    *out = logits;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_logits_close(yvex_logits *logits)
{
    free(logits);
}

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits)
{
    return logits ? logits->summary.status : YVEX_LOGITS_STATUS_EMPTY;
}

const char *yvex_logits_status_name(yvex_logits_status status)
{
    switch (status) {
    case YVEX_LOGITS_STATUS_EMPTY: return "empty";
    case YVEX_LOGITS_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_LOGITS_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err)
{
    if (!logits || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_get_summary", "logits and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &logits->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}



char *yvex_runtime_strdup(const char *text)
{
    char *copy;
    size_t len;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text)
{
    if (!out || cap == 0) {
        return;
    }
    snprintf(out, cap, "%s", text ? text : "");
}

void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph)
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



static void set_session_reason_from_graph(yvex_session *session)
{
    const yvex_graph *graph = yvex_engine_graph(session->engine);

    yvex_runtime_set_graph_reason(session->reason, sizeof(session->reason), graph);
}

static yvex_session_state reset_state_for_graph(const yvex_session *session)
{
    return session->graph_partial ? YVEX_SESSION_STATE_PARTIAL : YVEX_SESSION_STATE_READY;
}

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
    if (yvex_backend_status_of(backend) != YVEX_BACKEND_STATUS_READY) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_create", "backend is not ready for engine/session layer sessions");
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

    rc = yvex_kv_cache_create(&session->kv, yvex_engine_model(engine), context_length, err);
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

yvex_session_state yvex_session_state_of(const yvex_session *session)
{
    return session ? session->state : YVEX_SESSION_STATE_CLOSED;
}

unsigned long long yvex_session_position(const yvex_session *session)
{
    return session ? session->position : 0;
}

unsigned long long yvex_session_context_length(const yvex_session *session)
{
    return session ? session->context_length : 0;
}

int yvex_session_get_summary(const yvex_session *session,
                             yvex_session_summary *out,
                             yvex_error *err)
{
    yvex_kv_summary kv_summary;
    yvex_logits_summary logits_summary;

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

    if (session->kv && yvex_kv_cache_get_summary(session->kv, &kv_summary, err) == YVEX_OK) {
        out->kv_status = yvex_kv_status_name(kv_summary.status);
        out->kv_bytes = kv_summary.bytes;
    } else {
        out->kv_status = "empty";
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

const char *yvex_session_diagnostic_reason(const yvex_session *session)
{
    return session ? session->reason : "";
}

int yvex_session_accept_tokens(yvex_session *session,
                               const yvex_tokens *tokens,
                               yvex_error *err)
{
    if (!session || !tokens) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_accept_tokens",
                       "session and tokens are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_accept_tokens", "session is closed");
        return YVEX_ERR_STATE;
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

    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason),
                                 "prefill runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_prefill",
                   "prefill runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

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
    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason),
                                 "decode runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_decode_next",
                   "decode runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

int yvex_session_cancel(yvex_session *session,
                        yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_cancel", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_cancel", "session is closed");
        return YVEX_ERR_STATE;
    }
    session->state = YVEX_SESSION_STATE_CANCELLED;
    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason), "session cancelled");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_session_reset(yvex_session *session,
                       yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_reset", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_reset", "session is closed");
        return YVEX_ERR_STATE;
    }

    session->position = 0;
    session->accepted_tokens = 0;
    session->rejected_tokens = 0;
    session->state = reset_state_for_graph(session);
    set_session_reason_from_graph(session);
    yvex_error_clear(err);
    return YVEX_OK;
}


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
