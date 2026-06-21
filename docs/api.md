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

B0 implements the core version/status/error/log surface plus the runtime
filesystem path and run-directory skeleton.

Current public headers:

```text
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h
include/yvex/fs.h
```

Current aggregate:

```c
#include <yvex/error.h>
#include <yvex/fs.h>
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

## Future API Families

The families below are design contracts, not implemented APIs:

```text
artifact/GGUF
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

## Future Artifact and GGUF API

C0 may introduce artifact and GGUF parser objects. Generic parser APIs must use
checked byte ranges, explicit status codes, and precise parser failure messages.

## Future Backend API

Backend APIs belong to `docs/backend-contract.md`. Generic backend headers must
not expose CUDA, Metal, ROCm, or provider-specific native handles.
