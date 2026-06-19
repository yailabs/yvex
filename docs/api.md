# YVEX API

This document extracts the public C API contract from `docs/spine.md`. The spine
remains authoritative.

## Rules

```text
opaque objects for runtime ownership
plain structs for config/options/data
every public object has create/open and free/close
every non-trivial function accepts yvex_error *err
no hidden YAI dependency
no CUDA types in generic headers
no model-family-specific types in generic headers
no fake success
no parser reads unchecked byte ranges
no backend-specific state leaks into core APIs
no TUI-specific types
```

## Object Families

```text
yvex_artifact
yvex_gguf
yvex_model
yvex_engine
yvex_backend
yvex_session
yvex_chat
yvex_server
```

## Status and Error

```c
typedef enum {
    YVEX_OK = 0,
    YVEX_ERR = -1,
    YVEX_ERR_NOMEM = -2,
    YVEX_ERR_IO = -3,
    YVEX_ERR_FORMAT = -4,
    YVEX_ERR_UNSUPPORTED = -5,
    YVEX_ERR_BACKEND = -6,
    YVEX_ERR_BOUNDS = -7,
    YVEX_ERR_STATE = -8,
    YVEX_ERR_CANCELLED = -9,
    YVEX_ERR_INVALID_ARG = -10
} yvex_status;

typedef struct {
    yvex_status code;
    char where[96];
    char message[256];
} yvex_error;
```

Required functions:

```c
void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
const char *yvex_status_name(yvex_status status);
```

## Artifact and GGUF

```c
typedef struct yvex_artifact yvex_artifact;
typedef struct yvex_gguf yvex_gguf;
typedef struct yvex_gguf_value yvex_gguf_value;

typedef struct {
    const char *path;
    int mmap_enabled;
    int readonly;
} yvex_artifact_options;

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *opt, yvex_error *err);
void yvex_artifact_close(yvex_artifact *artifact);

int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err);
void yvex_gguf_close(yvex_gguf *gguf);
```

Iteration:

```c
uint64_t yvex_gguf_metadata_count(const yvex_gguf *gguf);
const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, uint64_t index);
const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, uint64_t index);
const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key);

uint64_t yvex_gguf_tensor_count(const yvex_gguf *gguf);
const yvex_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, uint64_t index);
const yvex_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name);
```

## Model, Engine, Session

```c
typedef struct yvex_model yvex_model;
typedef struct yvex_engine yvex_engine;
typedef struct yvex_session yvex_session;

int yvex_model_open(yvex_model **out, const yvex_model_options *opt, yvex_error *err);
void yvex_model_close(yvex_model *model);

int yvex_engine_open(yvex_engine **out, const yvex_engine_options *opt, yvex_error *err);
void yvex_engine_close(yvex_engine *engine);

int yvex_session_prefill(yvex_session *session, const yvex_tokens *tokens, yvex_error *err);
int yvex_session_eval(yvex_session *session, int token, yvex_error *err);
int yvex_session_decode_next(yvex_session *session, const yvex_sampling_options *opt, int *out_token, yvex_error *err);
int yvex_session_rewind(yvex_session *session, uint32_t pos, yvex_error *err);
```

Session state transitions and ownership are defined in `docs/spine.md` and are
not optional.

## Backend

```c
typedef enum {
    YVEX_BACKEND_CPU = 0,
    YVEX_BACKEND_CUDA = 1,
    YVEX_BACKEND_METAL = 2,
    YVEX_BACKEND_ROCM = 3
} yvex_backend_kind;

typedef struct yvex_backend yvex_backend;
typedef struct yvex_device_tensor yvex_device_tensor;

int yvex_backend_open(yvex_backend **out, const yvex_backend_options *opt, yvex_error *err);
void yvex_backend_close(yvex_backend *backend);
int yvex_backend_alloc(yvex_backend *backend, yvex_device_tensor **out, uint64_t bytes, yvex_error *err);
void yvex_backend_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor);
int yvex_backend_sync(yvex_backend *backend, yvex_error *err);
```

Generic headers must not expose CUDA or other backend-specific native types.

## Tokenizer, Prompt, Sampler

```c
typedef struct {
    int *ids;
    uint32_t len;
    uint32_t cap;
} yvex_tokens;

int yvex_tokenize(const yvex_model *model, const char *text, yvex_tokens *out, yvex_error *err);
int yvex_detokenize(const yvex_model *model, const int *ids, uint32_t n, char **out, yvex_error *err);
void yvex_tokens_free(yvex_tokens *tokens);

int yvex_sample_logits(const float *logits, uint32_t vocab, const yvex_sampling_options *opt, int *out_token, yvex_error *err);
```

Sampler behavior must be deterministic for identical logits, options, seed, and
implementation version.

## Event API

The event stream is the common surface for CLI streaming, JSONL, tracing, and
future server streaming.

```c
typedef enum {
    YVEX_EVENT_LOAD_START,
    YVEX_EVENT_LOAD_DONE,
    YVEX_EVENT_PREFILL_START,
    YVEX_EVENT_PREFILL_PROGRESS,
    YVEX_EVENT_PREFILL_DONE,
    YVEX_EVENT_DECODE_START,
    YVEX_EVENT_TOKEN,
    YVEX_EVENT_DECODE_DONE,
    YVEX_EVENT_ERROR,
    YVEX_EVENT_STATS
} yvex_event_kind;
```

Event schemas are versioned in CLI/JSONL output.
