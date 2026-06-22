# YVEX API

This document owns public C API contracts. `docs/spine.md` remains the broader
technical authority.

## API Claim Rule

```text
headers expose only implemented functions
future API families may be documented here but are not support claims
no CUDA/native backend type appears in generic public headers
no YAI case/control type appears in YVEX headers
no model-family-specific public type appears before implementation
every non-trivial function reports precise status/error behavior
```

## Current Implemented API

E0 implements the core version/status/error/log surface, runtime filesystem
paths/run directories, artifact byte views, range checks, GGUF header/probe,
metadata, raw tensor directory parsing, dtype/qtype storage accounting, YVEX
tensor table rows, a descriptor-only model summary, tokenizer metadata/vocab
tables, fixture tokenizer encode/decode, and default prompt rendering.

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

This is a name-mapping surface only. A logging sink, trace stream, metrics API,
and runtime configuration are future work.

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

## Future API Families

The families below are design contracts, not implemented APIs:

```text
graph/planner
memory plan
backend/device tensor
KV cache
session
sampler
events/trace/metrics/profile
server/provider
```

Future headers may be added only when the corresponding implementation, tests,
failure behavior, and documentation are delivered in the same wave.

## Future Backend API

Backend APIs belong to `docs/backend-contract.md`. Generic backend headers must
not expose CUDA, Metal, ROCm, or provider-specific native handles.
