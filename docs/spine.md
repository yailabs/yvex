# YVEX Implementation Spine

Date: 2026-06-19
Status: canonical pre-implementation spine
Project name: YVEX
Expansion: YAI Vector Execution
Implementation language: C
Primary platform: Linux + CUDA + NVIDIA DGX Spark / GB10
Secondary future platforms: CPU reference, Apple Metal, AMD/ROCm
Primary role: local inference engine / local provider runtime
User-facing interface policy: CLI-only
Terminal policy: rich CLI, REPL, streaming, status lines, JSON/JSONL; no TUI
Boundary relation: YVEX executes models; YAI controls operational use of model output
Agent system: outside YVEX, inside YAI
Initial implementation mode: code-first, no fake support, no fake inference, no fake provider claims

This document is the canonical implementation spine for the repository operated
locally as `yvex`. The public technical project is YVEX.

This spine replaces the previous public framing, the old external-node center
of gravity, and the docs-only validation posture.

## 0. Authority

This file is the reference for:

```text
project identity
repository reset
documentation cutover
public C API
runtime ownership
filesystem layout
CLI runtime
interactive CLI chat
model loading
GGUF parsing
tokenization
graph planning
backend execution
CUDA/DGX path
KV cache
prefill/decode
sampling
metrics/tracing
server/provider surface
YAI boundary
roadmap taxonomy
delivery-box execution method
```

Any older document that conflicts with this file is non-authoritative. P0.6
removed the old scaffold surface from the active repository.

## 1. Current Repository State

Repository root:

```text
/home/mothx/computer-science/projects/YAI/yvex
```

Git state at intake:

```text
branch: main
commit: 9ff28e1
remote: https://github.com/yailabs/yvex.git
```

Current active surface after A0.2:

```text
.gitignore
LICENSE
Makefile
NOTICE.md
README.md
docs/README.md
docs/api.md
docs/backend-contract.md
docs/cli-runtime.md
docs/roadmap.md
docs/runtime-filesystem.md
docs/spine.md
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h
include/yvex/fs.h
src/core/version.c
src/core/status.c
src/core/error.c
src/core/log.c
src/fs/paths.c
src/fs/run_dir.c
cli/yvex_cli.c
tests/test_status.c
tests/test_error.c
tests/test_version.c
tests/test_log.c
tests/test_fs.c
tests/test_cli.sh
```

Current Makefile behavior:

```text
make info
  prints YVEX A0.1 core/CLI status

make check
  checks the reduced canonical docs exist
  checks old scaffold surfaces are absent
  checks no forbidden terminal UI implementation paths exist
  rejects fake maturity claims in README.md
  builds libyvex.a
  builds build/bin/yvex
  runs core and filesystem tests
  runs CLI smoke tests
```

Current implementation state:

```text
public core headers implemented: version/status/error/log
runtime filesystem header implemented: fs
core implementation exists: version/status/error/log
filesystem implementation exists: paths/run_dir
libyvex.a builds
build/bin/yvex builds
implemented CLI commands: info/help/commands/version/paths
no yvexd server
no GGUF parser
no tokenizer
no backend implementation
unit tests exist for status/error/version/log/fs
no fixtures
no benchmark harness
```

Current terminal-interface guardrail state:

```text
no tui/ directory found
no include/yvex/tui.h found
no src/tui/ found
no ncurses references found
no dashboard references found
no panel references found
no alternate-screen references found
```

The reset does not need to unwind an existing TUI. It must instead install a
hard guardrail that one is not introduced.

## 2. Legacy Surface Purge

P0.6 removed the old scaffold surface from the active repository.

Active repository surface keeps:

```text
LICENSE
.gitignore
README.md
NOTICE.md
Makefile
docs/README.md
docs/spine.md
docs/roadmap.md
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
initial C core headers, source, CLI, and tests
```

Removed surfaces:

```text
old spine/reference docs
old integration docs
old benchmark docs
old single-topic scaffold docs replaced by canonical YVEX docs
benchmark/example/protocol folders with no implementation role
README-only source/test stubs
```

Purge guardrails:

```text
old scaffold directories must not return
old README-only source/test stubs must not return
tui/
src/tui/
include/yvex/tui.h
docs/tui.md
docs/tui-*.md
docs/terminal-layout.md when it implies TUI
panel_*.c
dashboard mode
ncurses dependency
alternate-screen terminal UI
persistent terminal panels
```

### 2.5 P0.2 Technical Weak-Area Map

P0.2 strengthens this spine before documentation cutover. The weak areas from
the implementation audit map to concrete additions as follows:

```text
artifact/GGUF parser
  -> binary layout rules, endian policy, metadata model, checked offset math,
     iteration APIs, malformed fixtures, parser failure classes

tokenizer/chat template
  -> tokenizer kind taxonomy, added/special tokens, BOS/EOS/stop policy,
     template source rules, deterministic golden tests

dtype/qtype/tensor layout
  -> block formulas, shape order, stride assumptions, backend qtype matrix,
     role classifier patterns, unsupported qtype report shape

graph IR
  -> op input/output contract, shape inference, tensor lifetime,
     topological execution, capability matching, graph golden dumps

memory planner
  -> KV byte formula, scratch reuse, mmap/resident policy, GPU OOM behavior,
     residency states, allocator arenas, memory-plan JSON sketch

backend/CUDA ABI
  -> device tensor ownership, stream and sync policy, CUDA error mapping,
     cuBLAS ownership, sm_121 compile target, parity contract

session lifecycle
  -> state machine, ownership, thread-safety, cancellation, rewind,
     multi-session and invalidation rules

sampler
  -> RNG contract, seed semantics, top-k/top-p ordering,
     NaN/Inf policy, logprobs and stop-token handling

CLI contract
  -> exit codes, argument grammar, TTY/non-TTY behavior, JSON schema version,
     JSONL event version, pipe-safe output

validation matrix
  -> unit tests, golden fixtures, malformed fixtures, future fuzzing,
     sanitizers, no-TUI guard, fake-claim scan, manual proof commands
```

## 3. Identity

Public name:

```text
YVEX
```

Expansion:

```text
YAI Vector Execution
```

Repository target:

```text
yvex
```

Binary names:

```text
yvex
yvexd
yvex_bench
```

Library:

```text
libyvex.a
```

Header root:

```text
include/yvex/
```

C prefix:

```text
yvex_
```

Runtime environment:

```text
YVEX_HOME
YVEX_CONFIG_DIR
YVEX_CACHE_DIR
YVEX_STATE_DIR
YVEX_RUN_DIR
```

Runtime filesystem:

```text
~/.config/yvex
~/.cache/yvex
~/.local/state/yvex
~/.local/share/yvex
.yvex/
```

## 4. Core Thesis

YVEX is a C local inference engine for open-weight models.

YVEX owns the execution path:

```text
model artifact
  -> file mapping
  -> format parser
  -> metadata table
  -> tensor table
  -> dtype/qtype system
  -> architecture profile
  -> tokenizer
  -> prompt renderer
  -> execution graph
  -> memory plan
  -> backend plan
  -> tensor residency
  -> KV cache
  -> prefill
  -> decode
  -> logits
  -> sampler
  -> token event stream
  -> metrics
  -> trace
  -> execution-local receipt
```

YVEX is not:

```text
YAI
governance system
actor system
agent runtime
workflow authority
tool approval layer
case state owner
memory authority
```

YVEX exposes local model execution as a provider/runtime boundary that YAI may
consume.

## 5. YVEX / YAI Boundary

YAI owns:

```text
case
subject
provider boundary
operation attempt
policy gate
control decision
effect boundary
observation boundary
receipt authority
record authority
projection
memory
actor context
tool/action approval
operator authority
workflow authority
case-bound durable state
```

YVEX owns:

```text
model file open/stat/mmap
format probe
GGUF parser
metadata table
tensor directory
tensor table
dtype/qtype registry
architecture profile
model validation
tokenizer
chat template
prompt rendering
token buffer
execution graph
memory plan
backend ABI
CPU reference backend
CUDA backend
future Metal backend
future ROCm backend
device tensor allocation
weight residency
KV cache
prefill
decode
logits buffer
sampler
token streaming
CLI rendering
local server response
execution-local trace
execution-local metrics
execution-local profile
execution-local receipt
```

Boundary flow:

```text
YAI case/control layer
  -> provider request
  -> YVEX execution
  -> token stream / response / metrics / trace / receipt
  -> YAI provider observation
  -> YAI receipt/record/projection/memory decision
```

YVEX must not:

```text
call YAI internally in the initial architecture
mutate YAI case state
decide case authority
approve actions
own actor memory
own tool execution policy
claim YAI integration without command-visible behavior
```

## 6. Interface Doctrine: CLI-only

YVEX is CLI-only.

Allowed terminal surfaces:

```text
plain CLI output
rich CLI output
interactive REPL
streamed token output
inline status lines
structured JSON output
structured JSONL streaming
logs on stderr
run artifacts on filesystem
```

Allowed implementation features:

```text
linenoise REPL
history
multiline input
slash commands
Ctrl+C cancellation
Ctrl+D exit
stdout/stderr split
one-line progress/status
human-readable rich sections
JSON output
JSONL events
```

Forbidden:

```text
TUI
terminal dashboard
ncurses
alternate screen
persistent terminal panels
terminal grid UI
GUI
--tui
--dashboard
```

The CLI is not a thin afterthought. It is the proof surface for implementation.
Every meaningful feature must have a command, a failure mode, and a validation
path.

## 7. Product Command Surface

Initial CLI binary:

```text
yvex
```

Primary commands:

```sh
yvex info
yvex inspect model.gguf
yvex metadata model.gguf
yvex tensors model.gguf
yvex plan model.gguf --backend cuda --ctx 32768
yvex graph model.gguf
yvex tokenize model.gguf --text "hello"
yvex detokenize model.gguf --ids 1,2,3
yvex prompt model.gguf --system "..." --user "..."
yvex experts model.gguf
yvex moe-plan model.gguf --backend cuda
yvex run --model model.gguf --backend cuda -p "Explain mmap in C" -n 128
yvex chat --model model.gguf --backend cuda
yvex bench --model model.gguf --backend cuda --prompt prompts/code.txt --tokens 256
yvex cuda-info
```

Server binary:

```text
yvexd
```

Server command:

```sh
yvexd --model model.gguf --backend cuda --host 127.0.0.1 --port 8080
```

Static library:

```text
libyvex.a
```

Consumers:

```text
yvex CLI
yvexd server
yvex_bench
tests
future embedded provider adapters
```

## 8. Target Repository Layout

Target root:

```text
yvex/
  Makefile
  README.md
  LICENSE
  NOTICE.md
  CONTRIBUTING.md

  include/yvex/
    yvex.h
    version.h
    status.h
    error.h
    log.h
    config.h
    fs.h
    artifact.h
    format.h
    gguf.h
    dtype.h
    tensor.h
    model.h
    arch.h
    tokenizer.h
    prompt.h
    chat_template.h
    graph.h
    op.h
    planner.h
    memory_plan.h
    backend.h
    device.h
    kv.h
    logits.h
    session.h
    sampler.h
    events.h
    metrics.h
    trace.h
    profile.h
    receipt.h
    chat.h
    terminal.h
    server.h
    yai_provider.h

  src/
    core/
    fs/
    artifact/
    formats/
    model/
    tokenizer/
    graph/
    backend/
    session/
    events/
    metrics/
    chat/
    terminal/
    server/
    yai/

  backends/
    cuda/
    metal/
    rocm/

  models/
    llama/
    qwen/
    deepseek/
    gemma/
    phi/
    kimi/
    glm/

  cli/
  server/
  bench/
  tests/
  fixtures/
  docs/
```

Required docs:

```text
docs/README.md
docs/spine.md
docs/roadmap.md
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
```

Forbidden layout:

```text
tui/
src/tui/
include/yvex/tui.h
docs/tui.md
docs/tui-*.md
docs/terminal-layout.md when terminal-layout means TUI
```

CLI-specific terminal docs are:

```text
docs/cli-runtime.md
```

## 9. Build System

Initial targets:

```text
make info
make lib
make cli
make test
make check
make clean
```

Future targets:

```text
make cpu
make cuda
make cuda-spark
make cuda-debug
make server
make bench
make sanitize
make release
```

Target behavior:

```text
make lib
  builds libyvex.a

make cli
  builds ./yvex

make test
  builds and runs C tests

make check
  runs lib + cli + tests + source hygiene checks

make cuda
  builds CUDA backend once backend code exists

make cuda-spark
  builds CUDA backend for DGX Spark / GB10 class once backend code exists

make server
  builds ./yvexd

make bench
  builds ./yvex_bench
```

Baseline validation must not require:

```text
model downloads
CUDA hardware
network access
server startup
YAI checkout
```

Baseline validation requires:

```text
C compilation
static library build
CLI build
unit tests
source hygiene checks
no fake support claims
no TUI path or dependency
```

## 9.5 Source and Code Quality Discipline

A0.2 folds source discipline rules into this spine. Keep the rules compact and
enforced by code review, tests, and `make check`.

Public headers:

```text
declare only implemented functions
avoid future object families until implementation exists
include no backend-native types in generic headers
include no YAI governance types
state ownership and non-goals in file comments
```

Core C:

```text
prefer fixed-size caller-owned objects for A0.1 helpers
use size_t for buffer capacities and byte counts
avoid heap allocation unless ownership is explicit
avoid hidden environment reads in core helpers
return deterministic names for known enums
handle unknown enum values deliberately
```

CLI:

```text
implemented commands live in a command table
unknown commands exit 2
future runtime commands are documented as future, not listed as implemented
stdout is for generated or machine-readable output
stderr is for logs, progress, warnings, and timing
```

Tests:

```text
core helpers require unit tests
CLI behavior requires smoke tests
unknown/unsupported behavior must be tested or documented
new support claims require tests and a manual proof command
```

## 10. Public API Rules

Rules:

```text
Opaque objects for runtime ownership.
Plain structs for config/options/data.
Every public object has create/open and free/close.
Every non-trivial function accepts yvex_error *err.
No hidden YAI dependency.
No CUDA types in generic headers.
No model-family-specific types in generic headers.
No fake success.
No parser reads unchecked byte ranges.
No backend-specific state leaks into core APIs.
No TUI-specific types.
No terminal dashboard API.
```

Core object families:

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

Core status:

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
```

Core error shape:

```c
typedef struct {
    yvex_status code;
    char where[96];
    char message[256];
} yvex_error;

void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
const char *yvex_status_name(yvex_status status);
```

Log domains:

```text
fs
loader
gguf
tensor
model
arch
tokenizer
prompt
graph
planner
memory
backend
cpu
cuda
metal
rocm
kv
prefill
decode
sampler
server
openai
yai
```

## 11. Runtime Filesystem

Environment variables:

```text
YVEX_HOME
YVEX_CONFIG_DIR
YVEX_CACHE_DIR
YVEX_STATE_DIR
YVEX_RUN_DIR
```

Default filesystem:

```text
~/.config/yvex/
  config.toml
  models.toml
  backends.toml

~/.cache/yvex/
  tokenizer/
  tensor-plans/
  graph-plans/
  compiled-kernels/
  downloads/

~/.local/state/yvex/
  runs/
  servers/
  locks/
  chat_history

~/.local/share/yvex/
  models/
  fixtures/
```

Project-local filesystem:

```text
.yvex/
  config.toml
  cache/
  runs/
  chat_history
```

Serious run directory:

```text
runs/YYYY-MM-DD/run_YYYYMMDD_HHMMSS_shortid/
  command.txt
  env.txt
  run.json
  model.json
  backend.json
  memory-plan.json
  graph-plan.txt
  graph-plan.json
  prompt.rendered.txt
  prompt.tokens.jsonl
  output.txt
  decode.tokens.jsonl
  logits/
  logprobs.jsonl
  metrics.json
  profile.json
  trace.jsonl
  receipt.json
  stderr.log
  stdout.log
```

The receipt is execution-local evidence. It is not a YAI case record until YAI
imports it.

## 12. Artifact and GGUF

Load phases:

```text
file.open
file.stat
file.mmap
format.probe
gguf.header
gguf.metadata
gguf.tensor_dir
tensor.table
arch.detect
model.profile
model.validate
memory.plan
backend.plan
```

Artifact API:

```c
typedef struct yvex_artifact yvex_artifact;

typedef struct {
    const char *path;
    int mmap_enabled;
    int readonly;
} yvex_artifact_options;

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *opt, yvex_error *err);
void yvex_artifact_close(yvex_artifact *artifact);

const char *yvex_artifact_path(const yvex_artifact *artifact);
uint64_t yvex_artifact_size(const yvex_artifact *artifact);
const void *yvex_artifact_data(const yvex_artifact *artifact);
int yvex_artifact_fd(const yvex_artifact *artifact);
```

Range rule:

```c
int yvex_range_check(uint64_t file_size, uint64_t offset, uint64_t len, yvex_error *err);
```

No parser reads unchecked bytes.

GGUF parser responsibilities:

```text
magic
version
tensor_count
metadata_count
metadata key/value table
tensor directory
alignment
tensor data base
tensor offset validation
dtype mapping
```

### 12.1 Binary Reading Contract

GGUF parsing is little-endian unless the format version explicitly defines a
different encoding. Initial YVEX rejects unexpected endian/layout variants with
`YVEX_ERR_UNSUPPORTED` rather than guessing.

Binary reads follow these rules:

```text
read only through checked cursor helpers
never cast mapped bytes directly to structs
never trust file-provided counts before overflow checks
check offset + length before every read
check count * element_size before allocation
check tensor data base + tensor offset before tensor access
reject trailing partial values
reject negative or impossible semantic sizes after unsigned conversion
preserve original file offsets in error reports
```

Required cursor helpers:

```c
int yvex_read_u8(yvex_byte_cursor *cur, uint8_t *out, yvex_error *err);
int yvex_read_u16le(yvex_byte_cursor *cur, uint16_t *out, yvex_error *err);
int yvex_read_u32le(yvex_byte_cursor *cur, uint32_t *out, yvex_error *err);
int yvex_read_u64le(yvex_byte_cursor *cur, uint64_t *out, yvex_error *err);
int yvex_read_f32le(yvex_byte_cursor *cur, float *out, yvex_error *err);
int yvex_read_bytes(yvex_byte_cursor *cur, const void **out, uint64_t len, yvex_error *err);
```

The cursor owns no file memory. It is a bounded view over `yvex_artifact`.

### 12.2 GGUF Metadata Contract

Metadata scalar kinds:

```text
uint8
int8
uint16
int16
uint32
int32
float32
bool
string
uint64
int64
float64
```

Metadata aggregate kinds:

```text
array<T>
array<array<T>>
```

Initial implementation may reject nested arrays deeper than two levels with a
precise unsupported error. It must still identify the key, declared type, array
depth, element count, and byte offset.

String rules:

```text
GGUF strings are length-prefixed byte ranges
parser validates length against file bounds
parser does not require UTF-8 for storage
human output escapes invalid UTF-8
JSON output replaces invalid sequences or emits escaped byte form
metadata keys must be valid non-empty byte strings
public metadata views remain valid until yvex_gguf_close
owned copies are made only when the model descriptor needs stable normalized data
```

Metadata iteration API:

```c
typedef struct yvex_gguf_value yvex_gguf_value;

uint64_t yvex_gguf_metadata_count(const yvex_gguf *gguf);
const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, uint64_t index);
const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, uint64_t index);
const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key);
```

### 12.3 Tensor Directory Contract

Tensor directory parsing must compute:

```text
name
rank
dimensions
GGUF type
YVEX dtype/qtype
file-relative tensor offset
absolute tensor data offset
logical element count
storage byte count
role if classifiable
```

Offset and alignment rules:

```text
tensor data base is computed after metadata and directory
tensor offset is relative to tensor data base
absolute offset = tensor data base + tensor offset
absolute offset must not overflow uint64_t
absolute offset + tensor bytes must be within file size
tensor data base must satisfy GGUF alignment metadata when present
each tensor offset must satisfy required alignment for the file
unknown alignment metadata is reported and defaults only when GGUF permits it
```

Tensor iteration API:

```c
uint64_t yvex_gguf_tensor_count(const yvex_gguf *gguf);
const yvex_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, uint64_t index);
const yvex_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name);
```

### 12.4 Malformed Fixture Requirements

C0 parser work requires fixtures for:

```text
empty file
short magic
bad magic
unsupported version
truncated header
metadata count overflow
tensor count overflow
string length beyond file
array element count overflow
nested array unsupported
tensor rank unsupported
tensor dims overflow element count
tensor byte-size overflow
tensor offset beyond file
tensor data base misaligned
duplicate tensor names
unsupported GGUF type
unsupported qtype
```

Each malformed fixture must have:

```text
fixture path
expected yvex_status
expected where prefix
expected stable message fragment
manual inspect command
```

### 12.5 Parser Failure Classes

Parser-specific failures:

```text
gguf.bad_magic
gguf.unsupported_version
gguf.truncated_header
gguf.truncated_metadata
gguf.unsupported_metadata_type
gguf.metadata_count_overflow
gguf.tensor_count_overflow
gguf.string_out_of_bounds
gguf.array_out_of_bounds
gguf.array_depth_unsupported
gguf.tensor_rank_unsupported
gguf.tensor_shape_overflow
gguf.tensor_offset_overflow
gguf.tensor_out_of_bounds
gguf.tensor_alignment
gguf.qtype_unsupported
```

GGUF commands:

```sh
yvex inspect model.gguf
yvex metadata model.gguf
yvex tensors model.gguf
```

Good failure example:

```text
YVEX_ERR_UNSUPPORTED
where: yvex_tensor_backend_plan
message: unsupported qtype IQ2_XXS for tensor blk.12.ffn.experts.37.down.weight role=moe_expert_down backend=cpu
```

## 13. Tensor, Model, Architecture

Dtypes and qtypes:

```text
unknown
f32
f16
bf16
q8_0
q4_0
q4_k
q5_k
q6_k
iq2_xxs
fp8
```

### 13.1 Dtype and Qtype Registry Contract

Every dtype/qtype entry is described by a registry row:

```c
typedef struct {
    yvex_dtype dtype;
    const char *name;
    uint32_t block_elems;
    uint32_t block_bytes;
    uint32_t scalar_bytes;
    int is_quantized;
    int requires_dequant;
} yvex_dtype_info;
```

Dense dtype byte size:

```text
elements = product(dims[0..rank-1])
bytes = elements * scalar_bytes
```

Block qtype byte size:

```text
elements = product(dims[0..rank-1])
blocks = ceil(elements / block_elems)
bytes = blocks * block_bytes
```

Overflow checks:

```text
rank must be in supported range
each dimension must be non-zero unless format explicitly permits zero
product(dims) must not overflow uint64_t
blocks calculation must not overflow
blocks * block_bytes must not overflow
absolute file offset + bytes must not overflow
```

Initial known qtype registry policy:

```text
f32, f16, bf16
  dense scalar entries

q8_0, q4_0
  simple block entries with fixed block_elems and block_bytes

q4_k, q5_k, q6_k
  K-quant entries described by explicit registry constants

iq2_xxs, fp8
  parseable only after byte layout is proven by fixture
```

No qtype may be marked supported for a backend until both byte-size accounting
and at least one operation path exist for that backend.

Backend qtype support matrix:

```text
dtype/qtype | parser | tensor table | CPU bytes | CPU op | CUDA bytes | CUDA op
f32         | yes    | yes          | planned   | planned| planned    | planned
f16         | yes    | yes          | planned   | planned| planned    | planned
bf16        | yes    | yes          | planned   | planned| planned    | planned
q8_0        | yes    | yes          | planned   | planned| planned    | planned
q4_0        | yes    | yes          | planned   | planned| planned    | planned
q4_k        | yes    | yes          | planned   | planned| planned    | planned
q5_k        | yes    | yes          | planned   | planned| planned    | planned
q6_k        | yes    | yes          | planned   | planned| planned    | planned
iq2_xxs     | watch  | watch        | unsupported | unsupported | unsupported | unsupported
fp8         | watch  | watch        | unsupported | unsupported | unsupported | unsupported
```

Matrix terms:

```text
yes
  implemented and covered by tests

planned
  explicitly not a support claim

watch
  format exists in the ecosystem, but YVEX has no support claim yet

unsupported
  command must fail with precise unsupported error
```

Tensor roles:

```text
token_embedding
output_norm
output_head
attention_norm
attention_q
attention_k
attention_v
attention_out
ffn_norm
ffn_gate
ffn_up
ffn_down
moe_router
moe_expert_gate
moe_expert_up
moe_expert_down
unknown
```

### 13.2 Tensor Shape and Storage Contract

Shape order:

```text
YVEX preserves GGUF dimension order in yvex_tensor_info.dims
dims[0] is the innermost contiguous dimension for GGUF storage accounting
architecture profiles may expose normalized logical shapes separately
public tensor table never silently transposes dimensions
```

Stride assumptions:

```text
model file tensors are contiguous byte ranges
initial tensor table does not expose arbitrary strided views
execution graph may create internal views later, but view tensors must carry
  explicit shape, stride, base tensor, offset, and lifetime
```

Padding and alignment:

```text
tensor byte count excludes inter-tensor file padding
tensor absolute offset must satisfy file alignment
memory planner may add backend-specific allocation padding
backend allocation padding must not change logical tensor byte count
```

Unsupported qtype report format:

```text
status: YVEX_ERR_UNSUPPORTED
where: yvex_tensor_backend_plan
message: unsupported qtype <qtype> for tensor <name> role=<role> backend=<backend> shape=<dims>
```

### 13.3 Tensor Role Classifier Contract

Role classifier inputs:

```text
architecture kind
tensor name
rank
dimensions
dtype/qtype
metadata hints
```

Initial name patterns:

```text
token_embedding
  token_embd.weight
  model.embed_tokens.weight
  tok_embeddings.weight

output_norm
  output_norm.weight
  model.norm.weight
  norm.weight

output_head
  output.weight
  lm_head.weight

attention_norm
  blk.*.attn_norm.weight
  model.layers.*.input_layernorm.weight

attention_q
  blk.*.attn_q.weight
  model.layers.*.self_attn.q_proj.weight

attention_k
  blk.*.attn_k.weight
  model.layers.*.self_attn.k_proj.weight

attention_v
  blk.*.attn_v.weight
  model.layers.*.self_attn.v_proj.weight

attention_out
  blk.*.attn_output.weight
  model.layers.*.self_attn.o_proj.weight

ffn_norm
  blk.*.ffn_norm.weight
  model.layers.*.post_attention_layernorm.weight

ffn_gate
  blk.*.ffn_gate.weight
  model.layers.*.mlp.gate_proj.weight

ffn_up
  blk.*.ffn_up.weight
  model.layers.*.mlp.up_proj.weight

ffn_down
  blk.*.ffn_down.weight
  model.layers.*.mlp.down_proj.weight

moe_router
  blk.*.ffn_gate_inp.weight
  model.layers.*.mlp.gate.weight

moe_expert_gate
  blk.*.ffn.experts.*.gate.weight

moe_expert_up
  blk.*.ffn.experts.*.up.weight

moe_expert_down
  blk.*.ffn.experts.*.down.weight
```

Classifier failures are not fatal by default. Unknown role is allowed for
inspection, but executable planning may reject required missing roles per
architecture profile.

Architecture classes:

```text
unknown
llama
qwen
deepseek
gemma
phi
kimi
glm
```

Every profile defines:

```text
required metadata
optional metadata
tensor naming patterns
required tensor roles
supported qtypes
tokenizer expectations
chat template expectations
attention structure
rope config
context length
hidden size
layer count
head count
KV head count
MoE shape if applicable
```

DeepSeek adds:

```text
expert count
active experts per token
router tensors
expert tensor roles
expert qtypes
expert residency planning
MoE metrics
```

Qwen adds:

```text
coder/instruct chat templates
large vocab handling
long-context assumptions
dense vs sparse variants
```

Kimi and GLM start as watch/inspectable unless concrete artifacts prove more.

## 14. Tokenizer and Prompt

Tokenizer API:

```c
typedef struct {
    int *ids;
    uint32_t len;
    uint32_t cap;
} yvex_tokens;

int yvex_tokenize(const yvex_model *model, const char *text, yvex_tokens *out, yvex_error *err);
int yvex_detokenize(const yvex_model *model, const int *ids, uint32_t n, char **out, yvex_error *err);
void yvex_tokens_free(yvex_tokens *tokens);
```

### 14.1 Tokenizer Contract

Tokenizer kinds:

```text
bpe
sentencepiece
unigram
byte_fallback
unknown
```

Tokenizer source precedence:

```text
GGUF tokenizer metadata
explicit tokenizer sidecar if command line allows it later
architecture default only for documented fixture models
unsupported error if required tokenizer data is absent
```

Vocabulary records must represent:

```text
token id
token bytes/text
token score when present
token type when present
normal token
added token
control token
user-defined token
byte token
unknown token
```

Special token registry:

```text
bos
eos
unk
pad
sep
cls
mask
fim_prefix
fim_middle
fim_suffix
chat control tokens
```

BOS/EOS policy:

```text
tokenizer reports whether BOS is added by default
prompt renderer decides whether chat templates already include BOS/EOS
no command silently inserts both tokenizer-default and template-provided BOS
EOS is used as a stop token only when model/profile says so or user requests it
```

Stop-token policy:

```text
stop token ids are resolved before decode
stop strings are matched after detokenization with byte-safe buffering
stop token itself is not emitted unless command requests raw token stream
JSONL token events mark stop_reason separately from emitted text
```

Detokenization constraints:

```text
detokenization must be byte-stable for known token pieces
invalid UTF-8 may be carried as bytes internally
human output uses replacement/escaping policy
JSON output must remain valid JSON
round-trip tests define expected lossless scope per tokenizer kind
```

### 14.2 Chat Template Contract

Template source precedence:

```text
explicit --template FILE later
GGUF tokenizer.chat_template
architecture/model-family default only when documented
minimal fallback only for fixture models
unsupported error for real chat models without a renderable template
```

Initial rendering policy:

```text
support a documented subset required by first target models
reject unsupported template constructs with key, construct, and offset/context
never eval arbitrary code from templates
never call shell or filesystem from template rendering
render to bytes/text before tokenization
preserve role boundaries for token debug output
```

Template data model:

```text
messages[]
  role
  content

options
  add_generation_prompt
  bos_token
  eos_token
  stop_tokens
  model_family
```

Future template engine policy:

```text
start with minimal safe renderer for known fixture/model templates
add a Jinja-compatible subset only when tests require it
document unsupported constructs explicitly
avoid embedding a large scripting runtime in A0-C0
```

Prompt roles:

```text
system
user
assistant
tool
```

Tokenizer commands:

```sh
yvex tokenize model.gguf --text "hello"
yvex detokenize model.gguf --ids 1,2,3
yvex prompt model.gguf --system "You are helpful" --user "Write C"
yvex dump-tokens model.gguf --prompt-file prompt.txt
```

Token debugging must expose:

```text
rendered prompt text
token IDs
token text pieces
role boundaries
BOS/EOS
stop tokens
context token count
chat template source
tokenizer metadata source
```

Many runtime failures originate from wrong templates, not kernels.

### 14.3 Tokenizer Golden Tests

Tokenizer work requires golden fixtures:

```text
raw text -> token ids
token ids -> text/bytes
chat messages -> rendered prompt text
rendered prompt text -> token ids
special token lookup
BOS/EOS insertion policy
stop-token resolution
byte fallback edge cases
invalid UTF-8 display policy
```

Each golden fixture must include:

```text
model/tokenizer source
input text or messages
expected token ids
expected token pieces when available
expected rendered prompt
expected stop ids
command used for manual proof
```

## 15. Graph, Planner, Backend

Graph operation kinds:

```text
embed
rms_norm
matmul
rope
attention_prefill
attention_decode
kv_write
kv_read
moe_router
moe_expert
swiglu
residual_add
logits
sampler
```

### 15.1 Graph Op Contract

Every graph op carries:

```text
op id
op kind
debug name
input value ids
output value ids
shape contract
dtype/qtype contract
backend capability requirements
scratch requirement estimate
source tensor references when applicable
layer index when applicable
```

Initial op semantics:

```text
embed
  inputs: token ids
  outputs: hidden[seq, hidden_size]

rms_norm
  inputs: hidden, weight
  outputs: hidden_norm with same shape as hidden

matmul
  inputs: lhs activation, rhs weight
  outputs: activation with inferred output feature dimension

rope
  inputs: q/k tensors, position ids, rope config
  outputs: rotated q/k tensors with same shape

attention_prefill
  inputs: q, k, v, mask/positions
  outputs: attention hidden for prefill chunk

attention_decode
  inputs: q, KV cache view, current position
  outputs: attention hidden for one or more decode tokens

kv_write
  inputs: k, v, position range
  outputs: updated KV cache state

kv_read
  inputs: KV cache state, position range
  outputs: k/v view

moe_router
  inputs: hidden, router weight
  outputs: expert ids and expert weights

moe_expert
  inputs: hidden, expert tensors, expert ids/weights
  outputs: combined expert hidden

swiglu
  inputs: gate/up activations
  outputs: activated hidden

residual_add
  inputs: lhs, rhs
  outputs: same shape hidden

logits
  inputs: final hidden, output head
  outputs: logits[vocab]

sampler
  inputs: logits, sampling options
  outputs: token id
```

### 15.2 Shape Inference and Lifetime

Shape inference rules:

```text
all graph values have rank, dims, dtype, and residency requirement
op builder validates input rank before creating outputs
shape errors include op id, op kind, input shape, and expected shape
dynamic sequence length is represented explicitly as seq or token_count
context length is a session property, not a tensor dimension guess
```

Tensor lifetime classes:

```text
model_weight
  loaded from artifact, lifetime equals engine/model

session_state
  KV cache and persistent decode state, lifetime equals session

activation
  temporary per prefill/decode step, lifetime planned by graph/memory planner

scratch
  backend temporary memory, reusable when lifetimes do not overlap

output
  logits/token events copied or streamed to caller
```

Execution order:

```text
graph is a DAG for one execution step
ops are emitted in topological order
cycles are invalid
side-effect ops such as kv_write declare state dependency edges
graph dump preserves op order and value ids
```

### 15.3 Backend Capability Matching

Capability matching input:

```text
op kind
input dtype/qtype
output dtype
tensor role
backend kind
device memory budget
architecture profile
```

Planning result:

```text
native backend op
backend op with dequant
CPU reference fallback
unsupported
```

CPU fallback policy:

```text
allowed for inspect/plan commands
allowed for tiny fixture execution
not silently allowed for full model run unless user requests fallback
must be visible in memory plan and graph dump
must affect support status
```

Graph dump expected format:

```text
graph <model> backend=<backend> status=<planned|partial|unsupported>
value v0 token_ids shape=[seq] dtype=i32 residency=host
op 000 embed inputs=[v0,w_token_embd] outputs=[v1] shape=[seq,hidden]
op 001 rms_norm inputs=[v1,w_attn_norm] outputs=[v2]
...
unsupported op 017 matmul reason="qtype q4_k missing cuda kernel"
```

Graph tests:

```text
fixture model descriptor -> expected op count
fixture descriptor -> expected topological dump
bad shape fixture -> expected shape error
missing tensor role fixture -> expected architecture validation error
unsupported backend fixture -> expected unsupported op line
```

Memory plan fields:

```text
model file bytes
mapped bytes
resident weight bytes
tensor bytes by dtype
tensor bytes by role
KV cache bytes
scratch bytes
temporary activation bytes
backend memory budget
host available memory
GPU total/free memory
residency decisions
unsupported memory constraints
offload candidates
expert residency estimate
```

### 15.4 Memory Planner Contract

KV byte formula:

```text
kv_bytes =
  n_layers *
  context_tokens *
  n_kv_heads *
  head_dim *
  2 *
  bytes_per_kv_element
```

Where:

```text
2 means K plus V
bytes_per_kv_element comes from KV dtype
batch or parallel sequence support must multiply explicitly when implemented
block/paged KV may add block metadata and padding
```

Scratch reuse strategy:

```text
planner computes activation lifetimes from graph order
non-overlapping temporary values may share arena ranges
scratch allocations are separated by backend residency
planner records peak bytes, not sum of all temporary tensors
```

Weight residency policy:

```text
mmap
  tensor bytes remain file-backed on host

resident_host
  tensor bytes copied into owned host memory

resident_pinned
  tensor bytes copied into pinned host memory when backend requires it

resident_device
  tensor bytes copied into device memory

streamed
  tensor bytes loaded on demand; initial policy marker only
```

Pinned host memory policy:

```text
not used by baseline CPU path
CUDA may use pinned staging buffers for large transfers
pinned allocation failure falls back to pageable host memory with warning
pinned bytes are tracked separately in memory metrics
```

GPU OOM strategy:

```text
detect during plan when possible from memory stats and estimated residency
detect during allocation with precise backend error
do not retry indefinitely
emit largest planned allocations in failure report
suggest lower ctx/offload only as informational text
never claim executable when required allocations failed
```

Allocator arenas:

```text
weights arena
KV arena
scratch arena
staging arena
logits/output arena
```

Fragmentation notes:

```text
planner tracks requested bytes and allocated bytes separately
backend reports peak allocated bytes when possible
large long-lived allocations happen before scratch allocations
future compaction is backend-specific and not assumed
```

Offload policy marker:

```text
offload is a planning result, not a hidden runtime trick
offloaded tensors list source residency and target residency
offload decisions must be visible in memory-plan.json
offload is unsupported until a command can prove it
```

Memory-plan JSON sketch:

```json
{
  "schema": "yvex.memory_plan.v1",
  "status": "planned",
  "backend": "cuda",
  "context_tokens": 32768,
  "weights": {
    "mapped_bytes": 0,
    "resident_host_bytes": 0,
    "resident_device_bytes": 0
  },
  "kv": {
    "dtype": "f16",
    "bytes_per_token": 0,
    "total_bytes": 0,
    "residency": "device"
  },
  "scratch": {
    "peak_bytes": 0,
    "arena_count": 0
  },
  "unsupported": []
}
```

Backend kinds:

```text
cpu
cuda
metal
rocm
```

Backend API responsibilities:

```text
init/destroy
device selection
memory stats
tensor allocation/free
host-to-device write
device-to-host read
op dispatch
sync
trace hooks
capability reporting
```

### 15.5 Backend ABI Contract

Device tensor ownership:

```text
yvex_backend_alloc creates yvex_device_tensor
yvex_backend_tensor_free destroys it
device tensor belongs to exactly one backend
device tensor records bytes, residency, alignment, debug name, and backend handle
core code treats backend handle as opaque
freeing with a different backend is invalid state
```

Device tensor state:

```text
allocated
written
ready
failed
freed
```

Async and sync boundaries:

```text
backend write may be synchronous in CPU backend
CUDA backend may enqueue async copies internally
public API is considered complete only after explicit sync or documented command boundary
CLI commands sync before reporting final success
trace events distinguish enqueue time from sync completion time where possible
```

Generic backend errors:

```text
allocation_failed
copy_failed
op_unsupported
op_failed
sync_failed
device_lost
invalid_tensor
```

### 15.6 CUDA ABI Contract

CUDA stream policy:

```text
one default execution stream per yvex_backend initially
separate copy stream may be added only with explicit sync edges
no implicit cross-session stream sharing until multi-session policy is implemented
all stream creation/destruction is owned by backend_cuda
```

CUDA error mapping:

```text
cudaErrorMemoryAllocation -> YVEX_ERR_BACKEND with backend memory context
cudaErrorInvalidValue -> YVEX_ERR_INVALID_ARG or YVEX_ERR_BACKEND by caller fault
cudaErrorNotSupported -> YVEX_ERR_UNSUPPORTED
cudaErrorLaunchFailure -> YVEX_ERR_BACKEND with kernel name
cudaErrorIllegalAddress -> YVEX_ERR_BACKEND with sync boundary and kernel name
unknown CUDA error -> YVEX_ERR_BACKEND with numeric code and cudaGetErrorString
```

cuBLAS ownership:

```text
backend_cuda owns cublasHandle_t
handle lifetime equals yvex_backend
handle is bound to backend execution stream
cuBLAS availability is reported by yvex cuda-info
matmul planner records whether an op uses cuBLAS or custom kernel
```

Compile targets:

```text
baseline cuda target is explicit in Makefile flags
DGX Spark / GB10 path uses sm_121 target when toolchain supports it
unsupported compiler/arch combination fails at build or cuda-info with clear message
fatbin/multi-arch support is future work
```

Backend capability matrix:

```text
op                | cpu | cuda | notes
tensor_alloc      | yes | planned | A/G before L
tensor_copy       | yes | planned | roundtrip fixture required
rms_norm          | planned | planned | CPU reference first
matmul_f16        | planned | planned | CUDA may use cuBLAS
matmul_q8_0       | planned | planned | dequant path required
matmul_q4_k       | planned | planned | qtype fixture required
attention_prefill | planned | planned | tiny fixture first
attention_decode  | planned | planned | KV fixture first
sampler           | planned | planned | deterministic parity
moe_router        | planned | planned | later MoE fixture
```

CPU/CUDA parity requirement:

```text
same fixture inputs
same seed when sampling is involved
documented tolerance per op
CPU output stored or generated as reference
CUDA output compared after sync
failure reports max absolute and relative difference
```

CPU reference exists for correctness:

```text
tensor allocation
tensor read/write
vector ops
rmsnorm reference
matmul baseline
sampler reference
tiny graph execution
one-token fixture inference
```

CUDA is the serious acceleration path:

```text
CUDA.0 device init + memory stats
CUDA.1 tensor alloc/write/read/copy
CUDA.2 vector kernels
CUDA.3 rmsnorm/silu/softmax/argmax
CUDA.4 F16/BF16 matmul via cuBLAS
CUDA.5 Q8_0 dequant/matmul
CUDA.6 Q4_K dequant/matmul
CUDA.7 attention prefill/decode
CUDA.8 KV cache GPU
CUDA.9 sampler helper
CUDA.10 MoE router
CUDA.11 expert cache/residency
```

Every CUDA op requires:

```text
CPU reference
fixed fixture
output tolerance
test command
timing
memory measurement
```

## 16. Session Runtime

Session owns:

```text
engine binding
backend binding
context size
current position
KV cache
logits buffer
sampling options
prefill chunk policy
trace hooks
cancel state
```

### 16.1 Session State Machine

Session states:

```text
created
loaded
prefilling
decoding
cancelled
failed
closed
```

Allowed transitions:

```text
created -> loaded
loaded -> prefilling
prefilling -> loaded
prefilling -> failed
prefilling -> cancelled
loaded -> decoding
decoding -> loaded
decoding -> failed
decoding -> cancelled
cancelled -> loaded only after explicit reset/rewind succeeds
failed -> closed
failed -> loaded only after explicit reset succeeds and backend state is valid
loaded -> closed
created -> closed
```

Invalid transitions return `YVEX_ERR_STATE` with current state, requested action,
and session position.

### 16.2 Ownership and Thread-Safety

Ownership rules:

```text
yvex_engine owns model descriptor, tensor table, graph/profile data
yvex_backend owns backend device/context resources
yvex_session borrows engine and backend
yvex_session owns KV cache, logits buffer, transcript token state, and counters
yvex_chat owns chat transcript text and user-facing turn state
```

Lifetime rules:

```text
engine must outlive sessions created from it
backend must outlive sessions using it
session close/free releases KV and logits before returning
closing engine while sessions exist is invalid
closing backend while sessions exist is invalid
```

Thread-safety policy:

```text
different sessions may run concurrently only after backend declares it supported
one session is single-owner and not thread-safe by default
callbacks/events are invoked on the thread driving the session
cancellation flag may be set from a signal-safe or minimal external path later
global logging must be internally synchronized when implemented
```

Core session API shape:

```c
typedef struct yvex_session yvex_session;

int yvex_session_prefill(yvex_session *session, const yvex_tokens *tokens, yvex_error *err);
int yvex_session_eval(yvex_session *session, int token, yvex_error *err);
int yvex_session_decode_next(yvex_session *session, const yvex_sampling_options *opt, int *out_token, yvex_error *err);
int yvex_session_copy_logits(yvex_session *session, float *out, uint32_t cap, yvex_error *err);
int yvex_session_rewind(yvex_session *session, uint32_t pos, yvex_error *err);
```

Future sync primitive:

```c
int yvex_session_sync(yvex_session *session, const yvex_tokens *canonical_prefix, yvex_error *err);
```

Purpose:

```text
cached prefix reuse
prompt edits
chat transcript continuation
rewind/rebuild
```

### 16.3 Cancellation, Rewind, and Invalidation

Cancellation behavior:

```text
Ctrl+C requests cancellation
session observes cancellation at defined boundaries:
  before chunk
  after backend sync
  before sampling
  before token emit
backend kernels are not assumed preemptible
cancelled prefill does not advance committed position
cancelled decode after emitted tokens keeps emitted tokens and marks stop_reason=interrupted
```

Rewind correctness:

```text
rewind target must be <= current position
KV cache is truncated or invalidated beyond target
logits are invalid after rewind unless target has corresponding cached logits
transcript token buffer is truncated to target
metrics record rewind count and invalidated token count
```

Invalidation rules:

```text
backend error invalidates in-flight op outputs
KV write failure invalidates current prefill/decode step
tokenizer/prompt render failure does not mutate session state
sampler failure does not mutate KV or position
graph/model mismatch invalidates session creation
```

Multi-session policy:

```text
M0/M1 may run one active session per backend by default
multiple CPU sessions are allowed only when memory ownership is independent
multiple CUDA sessions require explicit stream/resource policy
server concurrency is unsupported until K0 defines queueing/session pool rules
```

## 17. Prefill, Decode, Sampling

Prefill pipeline:

```text
render prompt
tokenize
resolve common prefix
plan chunks
for each chunk:
  embed
  layer loop
  attention prefill
  KV write
  FFN/MoE
  residual
produce logits for last token
```

Decode pipeline:

```text
sample/select next token from current logits
append token to transcript
eval token
update KV
compute next logits
emit token event
update metrics
repeat until stop
```

Sampling modes:

```text
greedy
temperature
top-k
top-p
min-p
repeat penalty later
banned tokens later
grammar constraints later
```

### 17.1 Sampler Determinism Contract

RNG policy:

```text
initial RNG is a named deterministic PRNG selected before sampler implementation
PRNG name and version are recorded in metrics/receipt
seed is uint64_t
same logits, same options, same seed, same implementation version must produce same token
```

Seed semantics:

```text
seed=0 is a valid deterministic seed unless command defines random-seed mode
random seed mode must record the resolved seed
chat session stores current sampler seed and step counter
rewind restores sampler step only when deterministic replay state is available
```

Temperature behavior:

```text
--greedy bypasses stochastic sampling
temperature == 0 behaves as greedy
temperature < 0 is invalid argument
NaN temperature is invalid argument
```

Logits hygiene:

```text
NaN logits are treated as invalid sampler input unless a command explicitly permits masking
+Inf wins over finite logits
-Inf is allowed as masked token
all -Inf logits returns YVEX_ERR_STATE with sampler context
vocab size must match model descriptor
```

Sampling order:

```text
apply logit masks and banned tokens when implemented
apply repeat penalty when implemented
apply temperature
compute probabilities
apply top-k
apply top-p
apply min-p
renormalize
sample with deterministic PRNG
```

Tie-breaking:

```text
greedy ties choose the lowest token id unless profile documents another rule
top-k sorting is by probability descending, then token id ascending
```

Logprobs:

```text
logprobs are computed from post-temperature, pre-filter distribution unless command requests raw logits
selected token logprob is included in token event when available
top logprobs preserve token id and detokenized piece when available
```

Stop-token handling:

```text
sampler may select a stop token
decode loop detects stop token before user-visible text emission by default
token event stream records stop_reason
raw mode may emit stop token event with explicit marker
```

Deterministic sampler tests:

```text
greedy simple max
greedy tie lowest id
temperature zero equals greedy
top-k fixed seed
top-p fixed seed
min-p fixed seed
NaN rejected
all masked rejected
selected logprob matches golden tolerance
```

Status lines:

```text
[prefill] chunk 2/4 | 1024/2048 tok | 384 tok/s | kv 1.2 GiB | elapsed 2.6s
[decode] 87 tok | 18.7 tok/s | last 52ms | p50 51ms | ctx 1192/32768
```

## 18. CLI Runtime

Modes:

```text
plain
rich
json
jsonl
```

Flags:

```text
--output plain
--output rich
--output json
--stream jsonl
--no-color
--quiet
--verbose
--status-line auto
--status-line off
--status-line always
```

Forbidden flags:

```text
--tui
--dashboard
```

stdout/stderr rule:

```text
stdout:
  generated text or machine-readable response

stderr:
  status
  progress
  logs
  warnings
  timing
```

### 18.1 CLI Exit Codes

Exit code contract:

```text
0   success
1   generic error
2   invalid command line arguments
3   filesystem/artifact error
4   format/parser error
5   unsupported model/qtype/backend feature
6   backend initialization or execution error
7   cancelled/interrupted
8   validation/test failure
9   internal invariant/state error
```

Every non-zero exit writes a human error to stderr. JSON/JSONL modes also emit a
machine-readable error object/event unless failure happens before output mode is
known.

### 18.2 Argument Parsing Grammar

Command shape:

```text
yvex <command> [positional...] [options...]
```

Rules:

```text
global options may appear before or after command when unambiguous
command-specific options are parsed after command selection
unknown option is exit 2
missing option value is exit 2
ambiguous positional arguments are exit 2
negative numeric values require explicit option context
paths are not expanded except by shell or documented config resolver
```

Initial commands must implement:

```text
yvex info
yvex --help
yvex <command> --help
yvex --version
```

### 18.3 TTY and Pipe-Safe Behavior

TTY detection:

```text
color auto-enables only when stderr/stdout target is a TTY
status line auto-enables only on TTY stderr
JSON and JSONL modes never emit color
non-TTY stdout receives plain generated text or machine-readable output only
progress and logs stay on stderr
```

Pipe-safe guarantees:

```text
yvex run ... > output.txt stores generated text only in default/plain mode
yvex ... --output json emits one JSON document on stdout
yvex ... --stream jsonl emits one JSON object per line on stdout
stderr may contain logs/status unless --quiet suppresses non-errors
status-line carriage returns are disabled when stderr is not a TTY unless forced
```

### 18.4 JSON and JSONL Versioning

JSON output envelope:

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "inspect",
  "status": "ok",
  "data": {},
  "error": null
}
```

JSON error envelope:

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "inspect",
  "status": "error",
  "data": null,
  "error": {
    "code": "YVEX_ERR_FORMAT",
    "where": "yvex_gguf_open",
    "message": "bad GGUF magic at offset 0"
  }
}
```

JSONL event envelope:

```json
{
  "schema": "yvex.event.v1",
  "event": "token",
  "run_id": "run_...",
  "ts_ns": 0,
  "data": {}
}
```

Event versioning rules:

```text
schema field is required
unknown fields may be ignored by consumers
existing field meaning must not change within same schema version
breaking event changes require new schema suffix
token text is valid JSON string after escaping/replacement policy
```

### 18.5 Status-Line Contract

Status-line modes:

```text
auto
off
always
```

Status-line rules:

```text
status line writes to stderr
status line uses carriage return only on TTY or always mode
final status line is followed by newline before command exit
status line never writes to stdout
JSON/JSONL stdout remains parseable while status line is active
```

Interactive chat:

```sh
yvex chat --model model.gguf --backend cuda
```

REPL features:

```text
linenoise
history
multiline input
slash commands
Ctrl+C cancellation
Ctrl+D exit
/read FILE
status line
streamed token output
```

Slash commands:

```text
/help
/model
/backend
/ctx N
/tokens N
/temp F
/top-k N
/top-p F
/min-p F
/seed N
/greedy
/stats
/memory
/tensors
/graph
/trace on
/trace off
/trace domains
/profile
/dump-tokens
/dump-logprobs N
/save-run
/reset
/rewind N
/read FILE
/quit
```

Rollback behavior:

```text
prefill failure:
  remove tentative user/assistant tokens
  preserve previous session if valid
  print precise error
  write failed run receipt

decode interrupted before first token:
  rollback tentative assistant start
  invalidate partial state if needed

decode interrupted after some tokens:
  keep partial assistant output
  mark stop_reason=interrupted
```

## 19. Logging, Tracing, Metrics

Trace flags:

```text
--quiet
--verbose
--log-level info
--trace gguf,tensor,backend
--trace all
--trace-prefill
--trace-decode
--trace-kv
--trace-sampler
--trace-out trace.jsonl
--metrics-out metrics.json
--profile-out profile.json
```

Metric groups:

```text
load
tensor
memory
prefill
decode
sampling
KV
MoE
server
```

Required load metrics:

```text
file_open_ms
mmap_ms
format_probe_ms
gguf_header_ms
metadata_parse_ms
tensor_dir_ms
tensor_table_ms
arch_detect_ms
model_validate_ms
backend_init_ms
total_load_ms
```

Required decode metrics:

```text
ttft_ms
decode_tokens
decode_ms
decode_tps
latency_p50_ms
latency_p90_ms
latency_p99_ms
sampler_ms_total
sampler_ms_avg
logits_ms_avg
token_eval_ms_avg
```

## 20. Model Ladder

YVEX uses a model ladder, not a jump straight to the largest model.

```text
M0 fixture models
  parser, tensor table, tokenizer mock, graph executor, sampler, run artifacts

M1 small real GGUF models
  real parser, tokenizer, prompt template, CPU path, first real token

M2 medium coder/instruct models
  practical local use, coding prompts, CUDA early value

M3 medium/large MoE models
  routing, expert residency, MoE memory behavior, expert cache

M4 DeepSeek V4 Flash-class target
  inspect, classify tensors, memory plan, KV estimate, unsupported report

M5 DeepSeek V4 Pro / trillion-class research
  research report only, no execution claim

M6 future semi-frontier watch
  Qwen, DeepSeek, Kimi, GLM, Llama, Gemma, Mistral, Phi
```

Support statuses:

```text
watch
documented
inspectable
tokenizable
planned
cpu-reference
cpu-one-token
cuda-partial
cuda-run
server-ready
unsupported
```

## 21. Server / Provider Surface

Server binary:

```sh
yvexd --model model.gguf --backend cuda --host 127.0.0.1 --port 8080
```

Initial endpoints:

```text
GET  /health
GET  /metrics
GET  /v1/models
POST /v1/completions
POST /v1/chat/completions
```

Later:

```text
POST /v1/embeddings
POST /v1/responses
```

Streaming uses SSE:

```text
data: {"choices":[{"delta":{"content":"Hello"}}]}
data: {"choices":[{"delta":{"content":" world"}}]}
data: [DONE]
```

YVEX exposes:

```text
plain HTTP provider
OpenAI-compatible subset
execution receipt
trace/metrics
run ID
model ID
```

YAI provides:

```text
case binding
provider attach
control decision
record import
projection/memory derivation
```

YVEX does not call YAI internally in early phases.

## 22. Failure Taxonomy

Failure classes:

```text
filesystem
artifact
format
metadata
tensor
dtype/qtype
architecture
tokenizer
prompt
graph
memory plan
backend init
backend memory
kernel
KV
prefill
decode
sampler
server
provider protocol
user interrupt
```

Every serious failure includes:

```text
status code
where
human message
optional tensor name
optional backend
optional model path
optional run id
```

Bad errors:

```text
invalid model
unsupported tensor
parse error
```

Good errors:

```text
GGUF metadata value for key tokenizer.chat_template has unsupported type array at offset X
tensor blk.17.attn_q.weight offset exceeds file bounds
tensor data base is not aligned to expected boundary
unsupported qtype IQ2_XXS for backend cpu
```

## 23. Roadmap Taxonomy

Pre-initial reset:

```text
P0    Repository reset and technical spine
P0.1  Current repo audit
P0.2  Spine technical densification pass
P0.3  Documentation and validation cutover plan
P0.4  Workspace / local namespace cutover
P0.5  Focused docs extraction
P0.6  Legacy surface purge
P0.7  Remote / origin cutover
P0.8  Runtime / system design
```

Code-first foundation:

```text
A0    C codebase skeleton
A0.1  Core skeleton maturity / file header discipline / CLI command contract
A0.2  Documentation consolidation / roadmap refoundation / code quality gate
```

Runtime and model tracks:

```text
B0 runtime filesystem
C0 artifact and GGUF
D0 tensor/model layer
E0 tokenizer and prompt
F0 graph and planner
G0 CPU backend
H0 engine/session runtime
I0 CLI/chat runtime
J0 metrics and tracing
K0 yvexd server/provider
L0 CUDA/DGX Spark backend
M0-M8 model support ladder
```

No TUI delivery exists.
No TUI implementation track exists.

## 24. Delivery Box Standard

Every wave uses:

```text
ID:
Title:
Goal:
Context:
Scope:
Non-goals:
Files to create:
Files to modify:
Public APIs:
Internal modules:
CLI commands:
Runtime files:
Tests:
Manual proof:
Failure modes:
Validation:
Report notes:
```

Example:

```text
ID: I0.1
Title: linenoise REPL skeleton
Goal: Create interactive yvex chat input loop without model execution.
Context: CLI runtime must be ready before inference so streaming layout is not retrofitted later.
Scope: cli/command_chat.c, src/chat/chat.c, src/terminal/status_line.c.
Non-goals: no tokenizer, no inference, no server, no TUI.
CLI commands: yvex chat --mock
Tests: test terminal command parser
Manual proof: yvex chat opens prompt, /help works, /quit exits.
Validation: make check
```

## 25. Validation Matrix

Baseline validation must not require:

```text
model downloads
CUDA hardware
network access
server startup
YAI checkout
```

Required validation layers:

```text
unit tests
  status/error/log
  range checks
  dtype byte formulas
  tensor role classifier
  sampler deterministic cases

golden fixtures
  tokenizer text -> ids
  ids -> text/bytes
  chat render -> prompt
  graph dump
  memory-plan JSON
  CLI JSON/JSONL envelopes

malformed parser fixtures
  short file
  bad magic
  unsupported version
  truncated metadata
  string out of bounds
  array overflow
  tensor shape overflow
  tensor offset out of bounds
  misaligned tensor data
  unsupported qtype

source hygiene
  no forbidden interface paths
  no fake support claims
  no direct parser struct casts over mapped bytes
  no unchecked range reads
  no backend-specific types in generic headers

manual proof commands
  make clean
  make check
  ./yvex info
  ./yvex inspect fixtures/gguf/tiny-header.gguf
  ./yvex inspect fixtures/gguf/bad-magic.gguf
```

Future validation layers:

```text
parser fuzzing
address sanitizer
undefined behavior sanitizer
thread sanitizer where useful
CUDA memcheck/sanitizer where available
CPU/CUDA parity fixtures
long-running decode soak tests
server streaming conformance tests
```

No-TUI guard:

```text
check repository paths for forbidden interface directories/files
check source for forbidden dependency names
check CLI help for forbidden modes
check docs for accidental product promises outside CLI-only policy
```

Fake-claim scan:

```text
reject unsupported claims for inference
reject unsupported claims for CUDA execution
reject unsupported claims for server/provider compatibility
reject benchmark claims without run artifacts
reject model support claims without commands and tests
```

P0.2 validation commands:

```sh
make check
git diff --check
LC_ALL=C grep -n '[^ -~]' docs/spine.md
git status --short
```

P0.2 manual proof must report:

```text
new headings added to docs/spine.md
weak-area audit mapped into concrete additions
no old docs modified
no runtime code added
areas intentionally deferred
```

## 26. First Implementation Target

After P0:

```text
A0 - Code-first C skeleton
```

Files:

```text
Makefile
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h
src/core/version.c
src/core/status.c
src/core/error.c
src/core/log.c
cli/yvex_cli.c
tests/test_status.c
tests/test_error.c
tests/test_version.c
tests/test_log.c
tests/test_cli.sh
```

Commands:

```sh
make clean
make check
make smoke
./build/bin/yvex info
```

Expected:

```text
libyvex.a builds
build/bin/yvex builds
tests run
no fake inference claims
no TUI code
no TUI docs
no TUI stubs
```

## 27. Engineering Principles

```text
Correctness before breadth.
Visibility before optimization.
CPU reference before CUDA-only correctness claims.
CUDA/DGX Spark first for serious acceleration.
CLI UX is part of the engine, not an afterthought.
No TUI.
No agents inside YVEX.
No YAI governance inside YVEX.
No fake provider support.
No fake model support.
No benchmark claims without reproducible commands.
Every failure names the exact missing capability.
Every serious run is reproducible from run artifacts.
Every parser read is range-checked.
Every CUDA op has CPU parity before trust.
Every support claim has a command.
```

## 28. Immediate Next Milestone

Next milestone after A0.2:

```text
B0 - Runtime Filesystem
```

B0 must:

```text
add XDG path resolution
add run directory creation
add cache/state/config directory helpers
add run.json writer skeleton
keep baseline validation independent of CUDA, network, model downloads, and YAI checkout
```

B0 must not:

```text
implement inference
declare GGUF behavior
declare CUDA behavior
declare OpenAI-compatible provider behavior
introduce TUI
introduce terminal UI dependencies
introduce dashboard or panel implementation
introduce fake provider support
introduce server/provider implementation
introduce CUDA implementation
```

Completed transition:

```text
P0.8 - Runtime / System Design
A0 - Code-first C skeleton
A0.1 - Core skeleton maturity / file header discipline / CLI command contract
A0.2 - Documentation consolidation / roadmap refoundation / code quality gate
```

Short target definition:

```text
YVEX is a C local inference engine for open-weight models. It is CUDA/DGX
Spark-first, CLI-native, transparent-by-default, and provider-ready. It owns
model loading, tokenizer/prompt rendering, tensor/model layout, execution
graph, backend runtime, KV cache, prefill/decode, logits, sampling, streaming
tokens, metrics and traces. It does not own YAI case/control/governance. YAI
consumes YVEX as a local provider boundary.
```

YVEX is CLI-only in this spine. It does not implement a TUI.
