# YVEX API

This document owns public C API contracts. `docs/spine.md` remains the broader
technical authority.

## API Claim Rule

```text
headers expose only implemented functions
future API families may be documented here but are not support claims
no native CUDA/Metal/ROCm handles appear in public headers
no YAI case/control type appears in YVEX headers
no model-family-specific public type appears before implementation
every non-trivial function reports precise status/error behavior
```

## Current Implemented API

K0 implements the core version/status/error/log surface, runtime filesystem
paths/run directories, artifact byte views, range checks, GGUF header/probe,
metadata, raw tensor directory parsing, dtype/qtype storage accounting, YVEX
tensor table rows, a descriptor-only model summary, tokenizer metadata/vocab
tables, fixture tokenizer encode/decode, default prompt rendering, graph
planning artifacts, shape helpers, estimate-only memory plans, plan objects,
backend ABI wrappers, the CPU reference backend, engine/session runtime
objects, KV/logits availability skeletons, CLI run/chat runtime shells, and
runtime metrics/trace/profile writers for implemented accepted-token paths.
It also implements the `yvexd` server shell API for health, metrics, and model
catalog status endpoints.

Current public headers:

```text
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h
include/yvex/fs.h
include/yvex/artifact.h
include/yvex/gguf.h
include/yvex/dtype.h
include/yvex/tensor.h
include/yvex/model.h
include/yvex/tokenizer.h
include/yvex/prompt.h
include/yvex/op.h
include/yvex/graph.h
include/yvex/memory_plan.h
include/yvex/planner.h
include/yvex/backend.h
include/yvex/engine.h
include/yvex/session.h
include/yvex/kv.h
include/yvex/logits.h
include/yvex/metrics.h
include/yvex/trace.h
include/yvex/profile.h
include/yvex/server.h
```

Current aggregate:

```c
#include <yvex/error.h>
#include <yvex/fs.h>
#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/dtype.h>
#include <yvex/tensor.h>
#include <yvex/model.h>
#include <yvex/tokenizer.h>
#include <yvex/prompt.h>
#include <yvex/op.h>
#include <yvex/graph.h>
#include <yvex/memory_plan.h>
#include <yvex/planner.h>
#include <yvex/backend.h>
#include <yvex/engine.h>
#include <yvex/session.h>
#include <yvex/kv.h>
#include <yvex/logits.h>
#include <yvex/metrics.h>
#include <yvex/trace.h>
#include <yvex/profile.h>
#include <yvex/server.h>
#include <yvex/log.h>
#include <yvex/status.h>
#include <yvex/version.h>
```

## Version

```c
#define YVEX_VERSION_MAJOR 0
#define YVEX_VERSION_MINOR 1
#define YVEX_VERSION_PATCH 0

const char *yvex_version_string(void);
int yvex_version_major(void);
int yvex_version_minor(void);
int yvex_version_patch(void);
```

The version helpers allocate no memory, read no environment variables, and do
not inspect build metadata.

## Status

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

const char *yvex_status_name(yvex_status status);
int yvex_status_is_ok(yvex_status status);
int yvex_status_is_error(yvex_status status);
```

`yvex_status_is_error(status)` intentionally treats every non-`YVEX_OK` value
as an error, including unknown positive values. Unknown names return
`YVEX_STATUS_UNKNOWN`.

## Error

```c
#define YVEX_ERROR_WHERE_CAP 96
#define YVEX_ERROR_MESSAGE_CAP 256

typedef struct {
    yvex_status code;
    char where[YVEX_ERROR_WHERE_CAP];
    char message[YVEX_ERROR_MESSAGE_CAP];
} yvex_error;

void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message);
void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
int yvex_error_is_set(const yvex_error *err);
yvex_status yvex_error_code(const yvex_error *err);
const char *yvex_error_where(const yvex_error *err);
const char *yvex_error_message(const yvex_error *err);
```

Error objects are caller-owned, fixed-size, null-terminated, and do not allocate.

## Log Names

```c
typedef enum {
    YVEX_LOG_ERROR,
    YVEX_LOG_WARN,
    YVEX_LOG_INFO,
    YVEX_LOG_DEBUG,
    YVEX_LOG_TRACE
} yvex_log_level;

typedef enum {
    YVEX_LOG_CORE,
    YVEX_LOG_CLI
} yvex_log_domain;

const char *yvex_log_level_name(yvex_log_level level);
const char *yvex_log_domain_name(yvex_log_domain domain);
```

This is a name-mapping surface only. A logging sink and runtime configuration
are future work. J0 metrics and trace APIs are separate runtime observability
surfaces.

## Runtime Filesystem

```c
#define YVEX_PATH_CAP 4096
#define YVEX_RUN_ID_CAP 64

typedef struct {
    char config_dir[YVEX_PATH_CAP];
    char cache_dir[YVEX_PATH_CAP];
    char state_dir[YVEX_PATH_CAP];
    char data_dir[YVEX_PATH_CAP];
    char project_dir[YVEX_PATH_CAP];
} yvex_paths;

typedef struct {
    char run_id[YVEX_RUN_ID_CAP];
    char root[YVEX_PATH_CAP];
    char command_path[YVEX_PATH_CAP];
    char stdout_path[YVEX_PATH_CAP];
    char stderr_path[YVEX_PATH_CAP];
    char metrics_path[YVEX_PATH_CAP];
    char trace_path[YVEX_PATH_CAP];
    char receipt_path[YVEX_PATH_CAP];
} yvex_run_dir;

int yvex_paths_default(yvex_paths *out, yvex_error *err);
int yvex_paths_project(yvex_paths *out, const char *project_root, yvex_error *err);
int yvex_paths_print(const yvex_paths *paths, FILE *fp, yvex_error *err);

int yvex_run_id_make(char *out, unsigned long cap, yvex_error *err);
int yvex_run_dir_prepare(yvex_run_dir *out, const yvex_paths *paths, const char *run_id, yvex_error *err);
int yvex_run_dir_create(const yvex_run_dir *run, yvex_error *err);
int yvex_run_dir_print(const yvex_run_dir *run, FILE *fp, yvex_error *err);
```

Filesystem APIs allocate no heap memory and use fixed caller-owned buffers.
Default path resolution requires `HOME`; project-local path construction uses
the explicit project root argument. Run directory creation creates directories
only and does not write run artifacts.

## Artifact

```c
#define YVEX_ARTIFACT_PATH_CAP 4096

typedef struct yvex_artifact yvex_artifact;

typedef struct {
    const char *path;
    int readonly;
    int map;
} yvex_artifact_options;

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *options, yvex_error *err);
void yvex_artifact_close(yvex_artifact *artifact);

const char *yvex_artifact_path(const yvex_artifact *artifact);
unsigned long long yvex_artifact_size(const yvex_artifact *artifact);
const unsigned char *yvex_artifact_data(const yvex_artifact *artifact);

int yvex_range_check(unsigned long long file_size,
                     unsigned long long offset,
                     unsigned long long len,
                     yvex_error *err);
```

The `map` option is accepted as a future policy flag, but YVEX currently uses an owned
read buffer and makes no mmap support claim. The data pointer remains valid
until `yvex_artifact_close`.

## GGUF Directory Parser

```c
#define YVEX_GGUF_MAGIC 0x46554747u
#define YVEX_GGUF_MAX_DIMS 4u

typedef struct {
    unsigned int version;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
} yvex_gguf_header;

typedef struct {
    int is_gguf;
    yvex_gguf_header header;
} yvex_gguf_probe;

typedef struct yvex_gguf yvex_gguf;
typedef struct yvex_gguf_value yvex_gguf_value;

typedef enum {
    YVEX_GGUF_VALUE_UINT8 = 0,
    YVEX_GGUF_VALUE_INT8 = 1,
    YVEX_GGUF_VALUE_UINT16 = 2,
    YVEX_GGUF_VALUE_INT16 = 3,
    YVEX_GGUF_VALUE_UINT32 = 4,
    YVEX_GGUF_VALUE_INT32 = 5,
    YVEX_GGUF_VALUE_FLOAT32 = 6,
    YVEX_GGUF_VALUE_BOOL = 7,
    YVEX_GGUF_VALUE_STRING = 8,
    YVEX_GGUF_VALUE_ARRAY = 9,
    YVEX_GGUF_VALUE_UINT64 = 10,
    YVEX_GGUF_VALUE_INT64 = 11,
    YVEX_GGUF_VALUE_FLOAT64 = 12
} yvex_gguf_value_type;

typedef struct {
    yvex_gguf_value_type element_type;
    unsigned long long count;
} yvex_gguf_array_info;

typedef struct {
    const char *name;
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_MAX_DIMS];
    unsigned int ggml_type;
    const char *ggml_type_name;
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
} yvex_gguf_tensor_info;

int yvex_gguf_probe_file(const yvex_artifact *artifact, yvex_gguf_probe *out, yvex_error *err);
int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err);

int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err);
void yvex_gguf_close(yvex_gguf *gguf);

const yvex_gguf_header *yvex_gguf_header_view(const yvex_gguf *gguf);
const char *yvex_gguf_value_type_name(yvex_gguf_value_type type);
```

C1 parses:

```text
magic          uint32 little-endian
version        uint32 little-endian
tensor_count   uint64 little-endian
metadata_count uint64 little-endian
metadata key/value table
raw tensor directory
tensor data base offset
alignment
```

The parsed `yvex_gguf` object is opaque. The caller owns the artifact and must
keep it alive for the parser object lifetime. C1 copies metadata keys, string
values, and tensor names so public views remain stable until `yvex_gguf_close`.

### GGUF Metadata

```c
unsigned long long yvex_gguf_metadata_count(const yvex_gguf *gguf);
const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key);

yvex_gguf_value_type yvex_gguf_value_type_of(const yvex_gguf_value *value);
int yvex_gguf_value_as_u64(const yvex_gguf_value *value, unsigned long long *out);
int yvex_gguf_value_as_i64(const yvex_gguf_value *value, long long *out);
int yvex_gguf_value_as_f64(const yvex_gguf_value *value, double *out);
int yvex_gguf_value_as_bool(const yvex_gguf_value *value, int *out);
int yvex_gguf_value_as_string(const yvex_gguf_value *value, const char **data, unsigned long long *len);
int yvex_gguf_value_array_info(const yvex_gguf_value *value, yvex_gguf_array_info *out);
```

Metadata behavior:

```text
out-of-range metadata index returns NULL
missing metadata key returns NULL
duplicate keys are allowed; find returns the first occurrence
metadata keys are copied as null-terminated strings
empty keys fail parse
string values are copied and length-preserving
arrays are parsed and exposed as element type/count summaries
nested arrays are rejected in C1 with YVEX_ERR_UNSUPPORTED
```

### GGUF Tensor Directory

```c
unsigned long long yvex_gguf_tensor_count(const yvex_gguf *gguf);
const yvex_gguf_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name);
unsigned long long yvex_gguf_tensor_data_offset(const yvex_gguf *gguf);
unsigned int yvex_gguf_alignment(const yvex_gguf *gguf);
```

Tensor directory behavior:

```text
raw GGUF tensor records only
out-of-range tensor index returns NULL
missing tensor name returns NULL
tensor names are copied as null-terminated strings
rank must be 1..YVEX_GGUF_MAX_DIMS
dimensions must be non-zero
dimension product overflow fails parse
relative tensor offset must satisfy alignment
absolute tensor offset is checked against file bounds
```

## Dtype / Qtype Registry

`include/yvex/dtype.h` owns the YVEX dtype/qtype vocabulary and storage
accounting formulas. It does not imply CPU, CUDA, or backend execution support.

```c
typedef enum {
    YVEX_DTYPE_UNKNOWN = 0,
    YVEX_DTYPE_F32,
    YVEX_DTYPE_F16,
    YVEX_DTYPE_BF16,
    YVEX_DTYPE_F64,
    YVEX_DTYPE_I8,
    YVEX_DTYPE_I16,
    YVEX_DTYPE_I32,
    YVEX_DTYPE_I64,
    YVEX_DTYPE_Q4_0,
    YVEX_DTYPE_Q8_0,
    /* full header also declares additional QK, IQ, TQ, and MXFP4 qtypes */
} yvex_dtype;

typedef struct {
    yvex_dtype dtype;
    const char *name;
    unsigned int ggml_type;
    unsigned int block_elems;
    unsigned int block_bytes;
    unsigned int scalar_bytes;
    int is_quantized;
    int is_supported_for_storage_accounting;
} yvex_dtype_info;

const yvex_dtype_info *yvex_dtype_get_info(yvex_dtype dtype);
const yvex_dtype_info *yvex_dtype_from_ggml_type(unsigned int ggml_type);
const char *yvex_dtype_name(yvex_dtype dtype);
int yvex_dtype_storage_bytes(yvex_dtype dtype,
                             unsigned long long element_count,
                             unsigned long long *out,
                             yvex_error *err);
```

D0 storage formulas cover dense numeric types plus `Q4_0` and `Q8_0`.
Other mapped qtypes may be recognized by name but return
`YVEX_ERR_UNSUPPORTED` for byte accounting until their formulas are delivered.

## Tensor Table

`include/yvex/tensor.h` builds YVEX-owned tensor table rows from C1 raw GGUF
tensor directory records.

```c
typedef struct yvex_tensor_table yvex_tensor_table;

int yvex_tensor_table_from_gguf(yvex_tensor_table **out,
                                const yvex_gguf *gguf,
                                yvex_error *err);
void yvex_tensor_table_close(yvex_tensor_table *table);

unsigned long long yvex_tensor_table_count(const yvex_tensor_table *table);
const yvex_tensor_info *yvex_tensor_table_at(const yvex_tensor_table *table,
                                             unsigned long long index);
const yvex_tensor_info *yvex_tensor_table_find(const yvex_tensor_table *table,
                                               const char *name);
const char *yvex_tensor_role_name(yvex_tensor_role role);
```

Tensor table rows preserve GGUF dimension order, copy tensor names, compute
element counts with overflow checks, map raw GGML types into YVEX dtypes, and
classify tensor roles by known LLM naming patterns. This is still inspection
and planning data, not backend allocation.

## Model Descriptor

`include/yvex/model.h` provides a descriptor-only model summary from parsed GGUF
metadata and a YVEX tensor table.

```c
typedef struct yvex_model_descriptor yvex_model_descriptor;

int yvex_model_descriptor_from_gguf(yvex_model_descriptor **out,
                                    const yvex_gguf *gguf,
                                    const yvex_tensor_table *tensors,
                                    yvex_error *err);
void yvex_model_descriptor_close(yvex_model_descriptor *model);

yvex_arch yvex_model_arch(const yvex_model_descriptor *model);
const char *yvex_arch_name(yvex_arch arch);
const char *yvex_model_name(const yvex_model_descriptor *model);
unsigned long long yvex_model_context_length(const yvex_model_descriptor *model);
unsigned long long yvex_model_tensor_count(const yvex_model_descriptor *model);
unsigned long long yvex_model_total_storage_bytes(const yvex_model_descriptor *model);
unsigned long long yvex_model_unsupported_tensor_accounting_count(const yvex_model_descriptor *model);
```

The descriptor reports architecture, optional model name, context length, tensor
count, known storage bytes, unsupported tensor accounting count, and role
counts. It is not a loaded model and does not own tokenizer, graph, backend,
session, or inference behavior.

## Tokenizer

`include/yvex/tokenizer.h` owns tokenizer metadata, vocabulary rows, special
token IDs, and fixture encode/decode. E0 does not execute generic Llama
SentencePiece, GPT-2 BPE, or HuggingFace tokenizer JSON.

```c
typedef struct yvex_tokenizer yvex_tokenizer;

int yvex_tokenizer_from_gguf(yvex_tokenizer **out,
                             const yvex_gguf *gguf,
                             const yvex_model_descriptor *model,
                             yvex_error *err);
void yvex_tokenizer_close(yvex_tokenizer *tokenizer);

yvex_tokenizer_kind yvex_tokenizer_kind_of(const yvex_tokenizer *tokenizer);
yvex_tokenizer_support yvex_tokenizer_support_of(const yvex_tokenizer *tokenizer);
const char *yvex_tokenizer_kind_name(yvex_tokenizer_kind kind);
const char *yvex_tokenizer_support_name(yvex_tokenizer_support support);

unsigned long long yvex_tokenizer_vocab_size(const yvex_tokenizer *tokenizer);
const yvex_token_info *yvex_tokenizer_token_at(const yvex_tokenizer *tokenizer,
                                               unsigned long long id);
```

Special token helpers return `YVEX_OK` when the ID is present and
`YVEX_ERR_UNSUPPORTED` when absent:

```c
int yvex_tokenizer_bos_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_eos_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_unk_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_pad_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_sep_id(const yvex_tokenizer *tokenizer, unsigned int *out);
```

Encode/decode is implemented only for `YVEX_TOKENIZER_KIND_FIXTURE_SIMPLE`:

```c
int yvex_tokenize_text(const yvex_tokenizer *tokenizer,
                       const char *text,
                       yvex_tokens *out,
                       yvex_error *err);

int yvex_detokenize_ids(const yvex_tokenizer *tokenizer,
                        const unsigned int *ids,
                        unsigned long long len,
                        char *out,
                        unsigned long long cap,
                        yvex_error *err);

void yvex_tokens_clear(yvex_tokens *tokens);
void yvex_tokens_free(yvex_tokens *tokens);
```

Support posture:

```text
yvex-fixture-simple: fixture encode/decode implemented
llama/gpt2/replit/rwkv: vocabulary visible, encode/decode unsupported
huggingface json: metadata visible, execution unsupported
unknown tokenizer: unsupported
```

## Prompt Rendering

`include/yvex/prompt.h` owns a deterministic E0 prompt renderer for explicit
role messages. It does not execute arbitrary Jinja chat templates.

```c
typedef enum {
    YVEX_PROMPT_ROLE_SYSTEM = 0,
    YVEX_PROMPT_ROLE_USER,
    YVEX_PROMPT_ROLE_ASSISTANT,
    YVEX_PROMPT_ROLE_TOOL
} yvex_prompt_role;

typedef struct {
    yvex_prompt_role role;
    const char *content;
} yvex_prompt_message;

int yvex_prompt_render(yvex_rendered_prompt *out,
                       const yvex_tokenizer *tokenizer,
                       const yvex_prompt_message *messages,
                       unsigned long long message_count,
                       const yvex_prompt_options *options,
                       yvex_error *err);

void yvex_rendered_prompt_free(yvex_rendered_prompt *prompt);
```

The E0 renderer emits the YVEX default role-tag format and can append an
assistant generation prompt. It allocates the rendered prompt and callers free
it with `yvex_rendered_prompt_free`.

## Graph and Ops

`include/yvex/graph.h` and `include/yvex/op.h` own the F0 graph planning
surface. Graphs are deterministic planning artifacts; they do not execute ops
or bind backend function pointers.

```c
typedef struct yvex_graph yvex_graph;

typedef struct {
    unsigned long long sequence_length;
    unsigned long long context_length;
    int include_decode_step;
    int include_prefill_path;
} yvex_graph_build_options;

int yvex_graph_build_for_model(yvex_graph **out,
                                const yvex_model_descriptor *model,
                                const yvex_tensor_table *tensors,
                                const yvex_graph_build_options *options,
                                yvex_error *err);
void yvex_graph_close(yvex_graph *graph);
```

Inspection APIs expose graph status, value count, op count, missing required
roles, value rows and op rows:

```c
yvex_graph_status yvex_graph_status_of(const yvex_graph *graph);
unsigned long long yvex_graph_value_count(const yvex_graph *graph);
unsigned long long yvex_graph_op_count(const yvex_graph *graph);
unsigned long long yvex_graph_missing_required_count(const yvex_graph *graph);
const yvex_graph_value_info *yvex_graph_value_at(const yvex_graph *graph,
                                                 unsigned long long index);
const yvex_graph_op_info *yvex_graph_op_at(const yvex_graph *graph,
                                           unsigned long long index);
```

F0 builds a partial fixture graph when token embedding exists but output norm
and output head are missing. Missing roles are diagnostics, not crashes.

## Shape Helpers

F0 exposes small checked shape helpers used by graph and memory planning tests:

```c
int yvex_shape_product(const unsigned long long *dims,
                       unsigned int rank,
                       unsigned long long *out,
                       yvex_error *err);
int yvex_shape_equal(const unsigned long long *a,
                     unsigned int a_rank,
                     const unsigned long long *b,
                     unsigned int b_rank);
int yvex_shape_copy(unsigned long long *dst,
                    unsigned int dst_cap,
                    const unsigned long long *src,
                    unsigned int src_rank,
                    yvex_error *err);
```

Ranks are bounded by `YVEX_GRAPH_MAX_DIMS`; zero dimensions and overflow fail
explicitly.

## Memory Plan

`include/yvex/memory_plan.h` owns estimate-only memory summaries. It performs no
allocation and no backend query.

```c
typedef struct yvex_memory_plan yvex_memory_plan;

int yvex_memory_plan_from_graph(yvex_memory_plan **out,
                                const yvex_graph *graph,
                                const yvex_tensor_table *tensors,
                                yvex_error *err);
void yvex_memory_plan_close(yvex_memory_plan *plan);

int yvex_memory_plan_get_summary(const yvex_memory_plan *plan,
                                 yvex_memory_plan_summary *out,
                                 yvex_error *err);
```

The accessor is named `yvex_memory_plan_get_summary` because C typedef names and
function names share the ordinary identifier namespace; the public summary type
keeps the requested `yvex_memory_plan_summary` name.

## Planner

`include/yvex/planner.h` combines a graph and memory plan. In G0, CPU backend
availability and capabilities are reported through the backend ABI, while CUDA
remains unsupported and execution remains disabled.

```c
typedef struct yvex_plan yvex_plan;

typedef struct {
    unsigned long long sequence_length;
    unsigned long long context_length;
    const char *backend_name;
} yvex_plan_options;

int yvex_plan_create(yvex_plan **out,
                     const yvex_model_descriptor *model,
                     const yvex_tensor_table *tensors,
                     const yvex_plan_options *options,
                     yvex_error *err);
void yvex_plan_close(yvex_plan *plan);
const yvex_graph *yvex_plan_graph(const yvex_plan *plan);
const yvex_memory_plan *yvex_plan_memory(const yvex_plan *plan);
```

`cpu`, `none`, and `cuda` are accepted plan labels. `cpu` reports
`backend_status: available`; `cuda` reports `backend_status: unsupported`.
No plan can report execution readiness as true in G0.

## Backend

`include/yvex/backend.h` owns the G0 backend ABI and CPU reference backend.
Backend and device tensor handles are opaque.

```c
typedef struct yvex_backend yvex_backend;
typedef struct yvex_device_tensor yvex_device_tensor;

int yvex_backend_open(yvex_backend **out,
                      const yvex_backend_options *options,
                      yvex_error *err);
int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err);
void yvex_backend_close(yvex_backend *backend);
```

The CPU backend is ready in G0. CUDA, Metal, and ROCm return
`YVEX_ERR_UNSUPPORTED`.

Memory stats use `yvex_backend_get_memory_stats`; the accessor is named this way
because `yvex_backend_memory_stats` is the public struct typedef.

```c
int yvex_backend_get_memory_stats(const yvex_backend *backend,
                                  yvex_backend_memory_stats *out,
                                  yvex_error *err);
```

Tensor allocation/read/write/copy:

```c
int yvex_backend_tensor_alloc(yvex_backend *backend,
                              const yvex_backend_tensor_desc *desc,
                              yvex_device_tensor **out,
                              yvex_error *err);
void yvex_backend_tensor_free(yvex_backend *backend,
                              yvex_device_tensor *tensor);

int yvex_backend_tensor_write(yvex_backend *backend,
                              yvex_device_tensor *tensor,
                              const void *src,
                              unsigned long long len,
                              yvex_error *err);
int yvex_backend_tensor_read(yvex_backend *backend,
                             const yvex_device_tensor *tensor,
                             void *dst,
                             unsigned long long len,
                             yvex_error *err);
int yvex_backend_tensor_copy(yvex_backend *backend,
                             yvex_device_tensor *dst,
                             const yvex_device_tensor *src,
                             yvex_error *err);
```

G0 tensor operations are full-buffer only. CPU allocation is zero-initialized
and tracked by allocated bytes, allocation count, and peak allocated bytes.

Capabilities:

```c
int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability);
const char *yvex_backend_capability_name(yvex_backend_capability capability);
```

G0 CPU capabilities:

```text
tensor_alloc: yes
tensor_read_write: yes
op_embed: yes
op_matmul: no
op_rms_norm: no
op_attention: no
```

Minimal CPU op:

```c
int yvex_backend_op_embed(yvex_backend *backend,
                          const yvex_device_tensor *embedding,
                          const unsigned int *token_ids,
                          unsigned long long token_count,
                          yvex_device_tensor *out,
                          yvex_error *err);
```

G0 supports only F32 embedding tensors. Embedding dims are `[hidden_size,
vocab_size]`; output dims are `[token_count, hidden_size]`. This is an op proof,
not model execution or inference.

## Engine

`include/yvex/engine.h` owns the H0 engine runtime object. An engine opens an
artifact, parses GGUF, builds a tensor table, builds a model descriptor, builds
a tokenizer when available, and may build the default graph. It does not own a
backend or session and does not execute.

```c
typedef struct yvex_engine yvex_engine;

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err);
int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err);
void yvex_engine_close(yvex_engine *engine);

int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err);
const char *yvex_engine_diagnostic_reason(const yvex_engine *engine);
```

The current fixture engine reports `partial` because the graph is missing
`output_norm` and `output_head`. `partial` is inspectable runtime state, not an
execution claim.

## Session

`include/yvex/session.h` owns the H0 lifecycle-only session object. A session
borrows an engine and backend, tracks state, exposes summaries, can accept
already-tokenized input into the session position, and reports unsupported
prefill/decode paths clearly.

```c
typedef struct yvex_session yvex_session;

int yvex_session_create(yvex_session **out,
                        const yvex_engine *engine,
                        yvex_backend *backend,
                        const yvex_session_options *options,
                        yvex_error *err);
void yvex_session_close(yvex_session *session);

int yvex_session_accept_tokens(yvex_session *session,
                               const yvex_tokens *tokens,
                               yvex_error *err);
int yvex_session_prefill(yvex_session *session,
                         const yvex_tokens *tokens,
                         yvex_error *err);
int yvex_session_decode_next(yvex_session *session,
                             unsigned int *out_token,
                             yvex_error *err);
```

In H0, `accept_tokens` updates counters and position when the context bound
allows it. `prefill` and `decode_next` return `YVEX_ERR_UNSUPPORTED`; no logits
are computed and no tokens are generated.

## KV And Logits Skeletons

`include/yvex/kv.h` and `include/yvex/logits.h` expose runtime availability
summaries. The current fixture lacks the attention/output-head facts needed to
size real KV/logits buffers, so both report `unavailable` and zero bytes.

```c
int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err);
int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err);

int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err);
int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err);
```

These objects are state and sizing skeletons only. They do not allocate backend
KV tensors, compute logits, expose a sampler, or imply generation support.

## CLI Runtime Shell

I0 adds `yvex run` and `yvex chat` as CLI runtime surfaces over the existing
engine/session APIs. I0 adds no new public C API. The chat runtime helpers under
`src/chat/` are private implementation code used by the CLI and tests.

The I0 runtime shell can:

```text
open engine/backend/session
tokenize fixture prompt text
accept prompt tokens into the session
report accepted-only status
render JSON result envelopes for yvex run
handle chat slash commands
```

The I0 runtime shell cannot:

```text
execute prefill
execute decode
compute logits
sample tokens
generate assistant text
claim inference readiness
```

## Metrics, Trace, And Profile API

J0 adds public observability APIs for work YVEX can actually perform.

Implemented:

```text
yvex_metrics
  phase timing for engine/session/runtime-shell phases
  prompt_tokens, accepted_tokens, rejected_tokens, chat_turns
  known_tensor_bytes and unsupported_tensor_accounting counters

yvex_trace
  JSONL event writer
  run_start/run_end, phase_start/phase_end, tokenize, accept_tokens, chat_turn

yvex_profile
  metrics.json writer
  profile.json writer
```

The J0 API does not expose or claim:

```text
decode tokens/sec
TTFT
generated token counters
CUDA timing
server status metrics
inference benchmarks
```

## Server Shell API

K0 adds `include/yvex/server.h` and the `yvexd` binary.

Implemented:

```text
yvex_server
yvex_server_options
yvex_server_summary
yvex_server_create
yvex_server_serve
yvex_server_stop
yvex_server_close
```

Implemented HTTP status endpoints:

```text
GET /health
GET /metrics
GET /v1/models
GET /
```

Generation endpoint requests return an unsupported JSON error in K0.

The K0 server shell does not implement:

```text
model generation
prefill/decode execution
streamed generated output
authentication
TLS
multi-client session pool
compatibility claim
```

## Future API Families

The families below are design contracts, not implemented APIs:

```text
sampler
```

Future headers may be added only when the corresponding implementation, tests,
failure behavior, and documentation are delivered in the same wave.

## Future Backend API

Backend APIs belong to `docs/backend-contract.md`. Generic backend headers must
not expose CUDA, Metal, ROCm, or provider-specific native handles.
