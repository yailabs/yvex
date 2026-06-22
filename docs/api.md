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

C1 implements the core version/status/error/log surface, runtime filesystem
paths/run directories, artifact byte views, range checks, and GGUF
header/probe, metadata, and raw tensor directory parsing.

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
```

Current aggregate:

```c
#include <yvex/error.h>
#include <yvex/fs.h>
#include <yvex/artifact.h>
#include <yvex/gguf.h>
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

C1 does not implement a YVEX tensor table, dtype/qtype byte-size registry,
model descriptor, tokenizer, backend, or inference path.

## Future API Families

The families below are design contracts, not implemented APIs:

```text
model/architecture
tokenizer/prompt
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
