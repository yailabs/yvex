# YVEX Implementation Spine

Date: 2026-06-19
Status: canonical technical and delivery spine
Project name: YVEX
Expansion: YAI Vector Execution
Implementation language: C
Primary platform: Linux + CUDA + NVIDIA DGX Spark / GB10
Secondary future platforms: CPU reference, Apple Metal, AMD/ROCm
Primary role: local inference engine / local provider runtime
User-facing interface policy: CLI-only
Boundary relation: YVEX executes models; YAI controls operational use of model output
Agent system: outside YVEX, inside YAI

YVEX is a C local inference engine for open-weight models. It is built as a
code-first runtime with a small public C API, a command-line proof surface,
strict parser safety, and no support claim without code, tests, and command
evidence.

## 0. Authority

`docs/spine.md` is the canonical YVEX technical and delivery spine.

It owns:

```text
current implementation state
completed milestones
planned milestones
acceptance gates
handoff rules
next authorized milestone
cross-cutting technical doctrine
```

No milestone may be renamed, split, reordered, or renumbered without editing
this spine in the same change.

Focused documents may narrow a technical area:

```text
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
```

If a focused document conflicts with this spine, this spine wins until the
focused document is reconciled.

## 1. Current State

Current phase:

```text
after C0
```

Current implementation commit:

```text
2818b6a
```

Next authorized milestone:

```text
C1 - GGUF metadata and tensor directory
```

Implemented surface:

```text
Core:
  include/yvex/version.h
  include/yvex/status.h
  include/yvex/error.h
  include/yvex/log.h
  src/core/version.c
  src/core/status.c
  src/core/error.c
  src/core/log.c

Runtime filesystem:
  include/yvex/fs.h
  src/fs/paths.c
  src/fs/run_dir.c

Artifact / GGUF header:
  include/yvex/artifact.h
  include/yvex/gguf.h
  src/artifact/artifact.c
  src/artifact/range.c
  src/formats/gguf.c

CLI:
  cli/yvex_cli.c
  implemented commands: info, help, commands, version, paths, inspect

Tests:
  tests/test_status.c
  tests/test_error.c
  tests/test_version.c
  tests/test_log.c
  tests/test_fs.c
  tests/test_artifact.c
  tests/test_gguf.c
  tests/test_cli.sh
```

Not implemented:

```text
GGUF metadata
GGUF tensor directory
dtype/qtype registry
tensor table
model descriptor
tokenizer
prompt rendering
graph planner
memory planner implementation
backend ABI implementation
CPU reference backend
session runtime
CLI run/chat
metrics/tracing implementation
server/provider
CUDA
model support ladder execution
inference
```

Interface policy:

```text
YVEX is CLI-only.
The only user-facing executable surface in the current repository is build/bin/yvex.
New interface surfaces require an explicit spine decision before implementation.
```

Implemented public surfaces:

```text
core version/status/error/log
filesystem paths/run directories
artifact byte view
GGUF header/probe
```

Planned public surfaces:

```text
GGUF full metadata/tensor object
dtype/qtype/tensor table
model descriptor
tokenizer/prompt
graph/planner
backend
session
chat/run
metrics/tracing
server/provider
```

## 2. Implementation Rules

Rules:

```text
YVEX is CLI-only.
Implemented commands must be command-visible and tested.
Public headers may declare only implemented APIs.
Future APIs belong in this spine until implementation exists.
Every wave must add tests for its claim.
No support claim is valid without code, tests, and command proof.
Library code does not print, abort, or hide errors.
Parser code reads only through checked range or cursor helpers.
Every parser read reports precise failure location and status.
Every C module keeps ownership, dependency, and validation comments current.
Every wave updates the delivery state before handoff.
```

### Current Command Surface

Current implemented commands:

```text
commands
help
info
inspect
paths
version
```

Future commands are listed only under the delivery that implements them.

## 3. Linear Delivery Spine

| ID | Status | Title |
|---|---|---|
| P0 | complete | Repository reset and technical spine |
| A0 | complete | C codebase skeleton |
| A0.1 | complete | Core skeleton maturity and CLI command contract |
| A0.2 | complete | Documentation consolidation and code-quality gate |
| B0 | complete | Runtime filesystem |
| C0 | complete | Artifact and GGUF parser base |
| C1 | next | GGUF metadata and tensor directory |
| D0 | planned | Tensor and model layer |
| E0 | planned | Tokenizer and prompt rendering |
| F0 | planned | Graph and planner |
| G0 | planned | CPU reference backend |
| H0 | planned | Engine and session runtime |
| I0 | planned | CLI run/chat runtime |
| J0 | planned | Metrics and tracing |
| K0 | planned | yvexd server/provider |
| L0 | planned | CUDA/DGX Spark backend |
| M0-M8 | planned | Model support ladder |

### P0 - Repository reset and technical spine

Status:

```text
complete
```

Owns:

```text
public YVEX identity
initial implementation spine
repository reset direction
YVEX naming
old scaffold rejection
CLI-only interface policy
YVEX/YAI boundary
```

Does not own:

```text
runtime C implementation
model parsing
backend execution
inference
```

Creates / modifies:

```text
README.md
NOTICE.md
Makefile
docs/README.md
docs/spine.md
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
```

CLI surface:

```text
none
```

Tests:

```text
documentation and guardrail checks only
```

Acceptance:

```text
spine exists
public framing is YVEX
old scaffold surface is removed
code stays out until A0
```

Handoff:

```text
A0 receives a code-first repository with a canonical spine.
```

### A0 - C codebase skeleton

Status:

```text
complete
```

Owns:

```text
first public core headers
first core C sources
static library build
CLI bootstrap
initial C tests
```

Does not own:

```text
filesystem runtime
artifact loading
GGUF parsing
model APIs
backend APIs
session APIs
inference
```

Creates / modifies:

```text
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
tests/test.h
Makefile
```

CLI surface:

```text
info
help
version
```

Tests:

```text
status tests
error tests
version tests
log tests
basic CLI smoke
```

Acceptance:

```text
libyvex.a builds
build/bin/yvex builds
core tests pass
headers expose only implemented APIs
```

Handoff:

```text
A0.1 hardens source style and command dispatch before runtime modules attach.
```

### A0.1 - Core skeleton maturity and CLI command contract

Status:

```text
complete
```

Owns:

```text
file-header discipline
public-header discipline
command table
CLI command metadata
implemented command list
CLI smoke tests
Makefile test-core/test-cli/smoke structure
```

Does not own:

```text
new runtime modules
future public APIs
model parsing
backend execution
```

Creates / modifies:

```text
Makefile
include/yvex/*.h
src/core/*.c
cli/yvex_cli.c
tests/*.c
tests/test_cli.sh
```

CLI surface:

```text
commands
help
info
version
```

Tests:

```text
core unit tests
CLI smoke test script
unknown command exit-code check
```

Acceptance:

```text
yvex commands works
yvex help works
unknown command exits 2
make check runs unit tests and smoke tests
```

Handoff:

```text
A0.2 reduces documentation sprawl and locks the code-quality gate.
```

### A0.2 - Documentation consolidation and code-quality gate

Status:

```text
complete
```

Owns:

```text
reduced documentation surface
progress state in canonical docs
code-quality gate before runtime filesystem work
current A0.1 code review
```

Does not own:

```text
new runtime functionality
new public runtime APIs
model parsing
```

Creates / modifies:

```text
docs/spine.md
docs/README.md
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
Makefile
```

CLI surface:

```text
none added
```

Tests:

```text
make check
make smoke
docs-tree validation
```

Acceptance:

```text
docs tree is reduced
make check validates reduced docs
no duplicate planning documents remain
runtime code remains unchanged
```

Handoff:

```text
B0 receives a focused repository and may implement the first runtime subsystem.
```

### B0 - Runtime filesystem

Status:

```text
complete
```

Owns:

```text
path resolution
environment override handling
project-local .yvex path construction
run directory preparation
run directory creation
paths CLI command
```

Does not own:

```text
config parser
artifact loading
model loading
run artifact writer beyond directory paths
```

Creates / modifies:

```text
include/yvex/fs.h
src/fs/paths.c
src/fs/run_dir.c
tests/test_fs.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/runtime-filesystem.md
docs/spine.md
```

CLI surface:

```text
yvex paths
yvex paths --project .
yvex paths --run
yvex paths --run --create
```

Tests:

```text
default path tests
environment override tests
project path tests
run ID tests
run directory create tests
CLI paths smoke tests
```

Acceptance:

```text
make check
make smoke
build/tests/test_fs
direct yvex paths proof
```

Handoff:

```text
C0 receives filesystem support and adds artifact byte views and GGUF header parsing.
```

### C0 - Artifact and GGUF parser base

Status:

```text
complete
```

Owns:

```text
artifact byte view
artifact open/close/size/data
range checks
GGUF magic/version/tensor_count/metadata_count header probe
inspect command in header-only mode
tiny malformed header fixtures
```

Does not own:

```text
metadata parser
tensor directory parser
model descriptor
tokenizer
backend
inference
```

Creates / modifies:

```text
include/yvex/artifact.h
include/yvex/gguf.h
src/artifact/artifact.c
src/artifact/range.c
src/formats/gguf.c
tests/test_artifact.c
tests/test_gguf.c
tests/fixtures/gguf/valid-minimal.gguf
tests/fixtures/gguf/bad-magic.gguf
tests/fixtures/gguf/short-header.gguf
tests/fixtures/gguf/unsupported-version.gguf
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex inspect <path>
```

Tests:

```text
artifact open/read tests
range-check tests
GGUF valid header tests
GGUF bad magic tests
GGUF short header tests
GGUF unsupported version tests
inspect smoke tests
```

Acceptance:

```text
valid minimal GGUF reports status: header-only
bad magic reports unknown/unsupported without crash
artifact and GGUF tests pass
make check passes
make smoke passes
```

Handoff:

```text
C1 receives artifact byte views, checked range helpers, and GGUF header parsing.
```

### C1 - GGUF metadata and tensor directory

Status:

```text
next
```

Owns:

```text
GGUF metadata key/value parser
supported metadata scalar values
supported string values
supported array cases
malformed metadata fixture coverage
GGUF tensor directory records
tensor name/rank/dim/type/offset parsing
tensor data offset and alignment checks
metadata and tensor inspection commands backed by parser code
```

Does not own:

```text
dtype/qtype registry beyond raw GGUF type identification needed for tensor directory
tensor byte formulas beyond safe directory bounds required in C1
model descriptor
tokenizer
graph
backend
inference
CUDA
server/provider
```

Creates / modifies:

```text
include/yvex/gguf.h
src/formats/gguf.c
tests/test_gguf.c
tests/fixtures/gguf/
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex metadata <file>
yvex tensors <file>
yvex inspect <file> may report parsed metadata and tensor directory counts
```

Tests:

```text
valid metadata fixture
valid tensor directory fixture
malformed metadata fixtures
malformed tensor directory fixtures
offset and alignment fixtures
lookup and iteration tests
CLI smoke for metadata/tensors
```

Acceptance:

```text
make clean
make check
make smoke
build/tests/test_gguf
yvex metadata valid fixture
yvex tensors valid fixture
malformed fixtures fail clearly
no model loading claim
no tokenizer claim
no backend claim
no inference claim
```

Handoff:

```text
D0 receives parsed metadata and tensor directory.
D0 builds dtype/qtype registry, tensor table, and model descriptor on top of C1.
```

### D0 - Tensor and model layer

Status:

```text
planned
```

Owns:

```text
YVEX dtype/qtype registry
tensor byte-size formulas
tensor info table
tensor role classifier
architecture profile skeleton
model descriptor skeleton
```

Does not own:

```text
tokenization
graph execution
backend kernels
session runtime
inference
```

Creates / modifies:

```text
include/yvex/dtype.h
include/yvex/tensor.h
include/yvex/model.h
src/model/dtype.c
src/model/tensor_table.c
src/model/descriptor.c
tests/test_dtype.c
tests/test_tensor_table.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/spine.md
```

CLI surface:

```text
yvex inspect may improve model/tensor summary only after implemented
```

Tests:

```text
dtype byte formula tests
qtype unsupported tests
tensor table tests
role classifier tests
model descriptor tests
```

Acceptance:

```text
byte-size formulas are tested
unsupported qtypes fail explicitly
tensor role classifier is tested
no backend claim
no inference claim
```

Handoff:

```text
E0 receives model metadata and tensor roles for tokenizer and prompt work.
F0 receives tensor/model shape data later for graph planning.
```

### E0 - Tokenizer and prompt rendering

Status:

```text
planned
```

Owns:

```text
tokenizer metadata extraction
tokenizer kind detection
special token registry
tokenization API
detokenization API
prompt rendering
chat template rendering subset
tokenizer fixtures and golden tests
```

Does not own:

```text
model execution
graph planning
backend execution
session runtime
```

Creates / modifies:

```text
include/yvex/tokenizer.h
include/yvex/prompt.h
src/tokenizer/
tests/test_tokenizer.c
tests/fixtures/tokenizer/
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex tokenize
yvex detokenize
yvex prompt
```

Tests:

```text
tokenize golden tests
detokenize golden tests
special token tests
prompt render tests
chat template subset tests
```

Acceptance:

```text
golden tokenization tests pass
prompt render tests pass
unsupported template constructs fail clearly
no model execution claim
```

Handoff:

```text
F0 receives tokenizer/prompt shape inputs for graph and planning work.
```

### F0 - Graph and planner

Status:

```text
planned
```

Owns:

```text
graph IR
op definitions
shape inference
tensor lifetime classes
memory planner skeleton
backend capability matching as plan data
graph and plan dumps
```

Does not own:

```text
actual backend kernels
session execution
generation loop
CUDA implementation
```

Creates / modifies:

```text
include/yvex/graph.h
include/yvex/op.h
include/yvex/planner.h
include/yvex/memory_plan.h
src/graph/
tests/test_graph.c
tests/test_memory_plan.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/spine.md
```

CLI surface:

```text
yvex graph <file>
yvex plan <file>
```

Tests:

```text
graph dump tests
shape inference tests
memory plan tests
unsupported op reporting tests
```

Acceptance:

```text
graph dumps are deterministic
memory plans are deterministic
unsupported operations are reported precisely
no execution claim
```

Handoff:

```text
G0 receives graph IR and planner output for CPU reference backend work.
```

### G0 - CPU reference backend

Status:

```text
planned
```

Owns:

```text
backend ABI first implementation
CPU tensor allocation
CPU tensor read/write
CPU reference kernels for minimal graph path
parity basis for future CUDA
```

Does not own:

```text
CUDA
server
interactive chat
large model performance claims
```

Creates / modifies:

```text
include/yvex/backend.h
src/backend/
tests/test_backend_cpu.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/backend-contract.md
docs/spine.md
```

CLI surface:

```text
yvex plan may report CPU backend capability after implemented
fixture-only execution command only if explicitly scoped
```

Tests:

```text
CPU backend allocation tests
CPU copy/read/write tests
minimal CPU op tests
fixture graph execution tests if scoped
```

Acceptance:

```text
CPU backend tests pass
backend failures return yvex_error
no CUDA claim
no broad inference claim
```

Handoff:

```text
H0 receives an implemented backend ABI for engine/session runtime.
L0 later uses CPU behavior as parity reference.
```

### H0 - Engine and session runtime

Status:

```text
planned
```

Owns:

```text
yvex_engine
yvex_session
session state machine
KV cache skeleton
logits buffer ownership
prefill/decode API skeleton over implemented backend behavior
```

Does not own:

```text
full chat UX
server process
CUDA backend
benchmark claims
```

Creates / modifies:

```text
include/yvex/session.h
include/yvex/kv.h
include/yvex/logits.h
src/session/
tests/test_session.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/spine.md
```

CLI surface:

```text
no chat command until I0 unless a fixture-only eval command is explicitly scoped
```

Tests:

```text
session lifecycle tests
state transition tests
KV ownership tests
rewind and invalidation tests when implemented
```

Acceptance:

```text
session lifecycle is tested
state failures are explicit
no broad inference claim
```

Handoff:

```text
I0 receives engine/session runtime for user-facing run and chat commands.
J0 receives runtime events for observability.
```

### I0 - CLI run/chat runtime

Status:

```text
planned
```

Owns:

```text
yvex run
yvex chat
REPL
slash commands
token event rendering
stdout/stderr discipline for generation
status lines
JSON/JSONL event stream if implemented
```

Does not own:

```text
server process
new backend kernels
CUDA
provider compatibility
```

Creates / modifies:

```text
src/chat/
src/terminal/
cli/yvex_cli.c
tests/test_cli_run.sh
tests/test_cli_chat.sh
Makefile
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex run
yvex chat
```

Tests:

```text
run command smoke tests
chat command parser tests
non-interactive output separation tests
stream event tests if implemented
```

Acceptance:

```text
command proof exists
non-TTY behavior is tested
generated output separation is tested
unsupported backend claims are absent
```

Handoff:

```text
J0 receives runtime token/event streams for metrics and tracing.
K0 receives a runtime path that can be served later.
```

### J0 - Metrics and tracing

Status:

```text
planned
```

Owns:

```text
metrics collector
trace JSONL
profile report
run metrics files
latency counters
token counters
```

Does not own:

```text
benchmark result claims
server implementation
new backend behavior
```

Creates / modifies:

```text
include/yvex/metrics.h
include/yvex/trace.h
src/metrics/
tests/test_metrics.c
tests/test_trace.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/spine.md
```

CLI surface:

```text
--trace
--metrics-out
--profile-out
```

Tests:

```text
metrics JSON tests
trace JSONL tests
profile output tests
run artifact path tests
```

Acceptance:

```text
JSON/JSONL output is valid
metrics are present for implemented runtime paths
no benchmark claim exists without reproducible artifacts
```

Handoff:

```text
K0 receives observability primitives for server/provider behavior.
L0 receives measurement hooks for backend parity and timing.
```

### K0 - yvexd server/provider

Status:

```text
planned
```

Owns:

```text
yvexd process
HTTP server skeleton
health endpoint
metrics endpoint
local provider boundary
future OpenAI-compatible subset only when implemented
```

Does not own:

```text
YAI governance
YAI case state
provider compatibility claims without tests
new model execution behavior
```

Creates / modifies:

```text
include/yvex/server.h
src/server/
server/yvexd.c
tests/test_server.c or shell smoke
Makefile
docs/api.md
docs/spine.md
```

CLI surface:

```text
yvexd --help
yvexd health or HTTP /health
```

Tests:

```text
server help tests
health endpoint tests
metrics endpoint tests when implemented
stream conformance tests when implemented
```

Acceptance:

```text
health endpoint is tested
model endpoints are claimed only after implementation
provider compatibility is tested before being claimed
```

Handoff:

```text
YAI may consume YVEX as a local provider boundary only after command-visible behavior exists.
```

### L0 - CUDA/DGX Spark backend

Status:

```text
planned
```

Owns:

```text
CUDA backend attachment
cuda-info command
device memory stats
CUDA tensor allocation
CPU/CUDA parity fixtures
first CUDA kernels as scoped subdeliveries
DGX Spark build target when toolchain support is proven
```

Does not own:

```text
CUDA support claims before tests
model support ladder execution by itself
server/provider behavior
```

Creates / modifies:

```text
backends/cuda/
tests/test_cuda_*.c or cuda smoke scripts
Makefile
cli/yvex_cli.c
docs/backend-contract.md
docs/spine.md
```

CLI surface:

```text
yvex cuda-info
backend cuda option only after tests prove it
```

Tests:

```text
cuda-info smoke tests
device memory stat tests
CPU/CUDA parity fixtures for each implemented op
CUDA failure mapping tests where practical
```

Acceptance:

```text
CUDA build target is explicit
first implemented CUDA op has CPU parity
device memory failures are explicit
no broad CUDA backend claim
```

Handoff:

```text
M0-M8 may use CUDA only for support levels backed by command proof.
```

### M0-M8 - Model support ladder

Status:

```text
planned
```

Owns:

```text
fixture models
small GGUF model support path
medium coder/instruct model inspection
MoE inspection
DeepSeek/Qwen/Kimi/GLM/Llama/Gemma/Phi watch and support states
DGX Spark feasibility matrix
model support freeze
```

Does not own:

```text
support claims without command proof
execution claims beyond implemented runtime/backend paths
remote model download assumptions inside baseline validation
```

Creates / modifies:

```text
tests/fixtures/models/ when small fixtures are introduced
docs/spine.md
docs/backend-contract.md when backend feasibility changes
CLI smoke tests for each support level
```

CLI surface:

```text
commands depend on the implemented support level
```

Tests:

```text
fixture model tests
small real model inspection/tokenization tests when available
generation tests only after backend/session support exists
```

Acceptance:

```text
each support level has command proof
large-model feasibility is reported as feasibility until execution is proven
no broad model support claim
```

Handoff:

```text
future model work must state the exact support level and proof command.
```

## 4. Completed Deliveries

### Completed Milestones

| ID | Commit | Result |
|---|---|---|
| P0 | b7c3dcf | Established YVEX pre-implementation spine and public identity. |
| P0.6 | f2e4890 | Purged legacy scaffold surface. |
| P0.7 | 70e8837 | Cut over remote identity to YVEX. |
| P0.8 | bbef021 | Defined runtime system design for A0. |
| A0 | 2620c59 | Added core C skeleton and CLI bootstrap. |
| A0.1 | 164ec95 | Hardened core skeleton style and CLI command contract. |
| A0.2 | 7e5879c | Consolidated docs and ran code-quality gate. |
| B0 | 8e7d6c8 | Added runtime filesystem paths and run directory skeleton. |
| C0 | 2818b6a | Added artifact layer and GGUF header probe. |

Current implemented CLI command set:

```text
commands
help
info
inspect
paths
version
```

Current validation baseline:

```text
make clean
make check
make smoke
git diff --check
```

## 5. Active / Next Delivery

Next authorized milestone:

```text
C1 - GGUF metadata and tensor directory
```

C1 is not split. C1 is not renamed. C1 starts only after this spine gate is
committed and validation passes.

C1 must produce:

```text
metadata parser
metadata iteration/lookups
tensor directory parser
tensor iteration/lookups
metadata CLI command
tensors CLI command
malformed fixture coverage
clear errors for unsupported metadata/tensor cases
```

C1 must not produce:

```text
model descriptor
tokenizer
backend
session runtime
generation command
server/provider behavior
CUDA backend
inference claim
```

Required C1 proof:

```text
make clean
make check
make smoke
build/tests/test_gguf
build/bin/yvex metadata <valid fixture>
build/bin/yvex tensors <valid fixture>
malformed fixture commands fail with precise errors
```

## 6. Planned Delivery Catalogue

The planned delivery catalogue is linear:

```text
D0 consumes C1 and creates tensor/model layer.
E0 consumes D0 and creates tokenizer/prompt rendering.
F0 consumes D0/E0 and creates graph/planner.
G0 consumes F0 and creates CPU reference backend.
H0 consumes G0/F0/E0 and creates engine/session runtime.
I0 consumes H0 and creates user-facing run/chat commands.
J0 consumes H0/I0 and creates metrics/tracing.
K0 consumes H0/I0/J0 and creates yvexd provider surface.
L0 consumes G0/J0 and creates CUDA/DGX Spark backend path.
M0-M8 consume the implemented runtime/backends and assign model support levels.
```

Each planned wave is defined in Section 3. A wave that is not defined there is
not authorized.

Do-not-proceed gates:

```text
Do not start D0 until C1 metadata and tensor directory tests pass.
Do not start E0 until D0 model descriptor surfaces exist.
Do not start F0 until D0 tensor/model shapes are available.
Do not start G0 until F0 graph/planner contracts exist.
Do not start H0 until at least one backend ABI path exists.
Do not start I0 until session lifecycle is implemented and tested.
Do not start K0 until runtime events and session behavior are stable enough to serve.
Do not start L0 until CPU reference parity path exists.
Do not assign model support levels without command proof.
```

## 7. Cross-Cutting Technical Doctrine

### Artifact/GGUF Doctrine

Implemented by:

```text
C0: artifact byte view and GGUF header/probe
C1: metadata and tensor directory
D0: dtype/qtype registry and tensor table
```

Rules:

```text
GGUF parsing is little-endian unless a later supported version says otherwise.
Parser code never casts mapped bytes directly to structs.
All binary reads go through checked range or cursor helpers.
All file-provided counts are overflow-checked before allocation or iteration.
Errors preserve enough context to identify the failing offset or field.
```

### Tensor/Model Doctrine

Implemented by:

```text
C1: raw tensor directory records
D0: dtype/qtype registry, tensor table, model descriptor
F0: graph shape use
G0/L0: backend capability use
```

Rules:

```text
Raw tensor directory parsing is not model support.
YVEX preserves file dimension order in raw tensor info.
Tensor byte formulas belong to dtype/qtype registry work.
Unsupported qtypes must fail with tensor name, role when known, and backend.
Model descriptors may only claim fields backed by metadata and tensor evidence.
```

### Tokenizer/Prompt Doctrine

Implemented by:

```text
E0: tokenizer and prompt rendering
I0: CLI run/chat usage
M0-M8: model-specific support levels
```

Rules:

```text
Tokenizer support is proven by golden tokenization and detokenization tests.
Prompt rendering support is proven by rendered prompt fixtures.
Template support starts as a bounded safe subset.
Unsupported template constructs fail clearly.
```

### Graph/Planner Doctrine

Implemented by:

```text
F0: graph IR, op semantics, shape inference, memory plan skeleton
G0: backend execution of planned CPU path
L0: CUDA capability matching and parity
```

Rules:

```text
Graph planning is not execution.
Each graph value carries rank, dims, dtype, and residency requirement.
Shape errors include op kind, input shape, and expected shape.
Backend capability matching must distinguish native, dequant, fallback, and unsupported.
```

### Backend/CUDA Doctrine

Implemented by:

```text
G0: CPU reference backend and backend ABI
J0: metrics/tracing hooks for backend measurements
L0: CUDA/DGX Spark backend
```

Rules:

```text
CPU reference comes before accelerated correctness claims.
Backend handles remain opaque outside backend modules.
CUDA work requires CPU parity fixture, tolerance, command proof, and memory reporting.
Device allocation failures return precise backend errors.
```

### Session/Runtime Doctrine

Implemented by:

```text
H0: engine/session lifecycle, KV/logits ownership, prefill/decode API skeleton
I0: run/chat loop and user-facing streaming
J0: runtime metrics and trace emission
```

Rules:

```text
Session state transitions are explicit and tested.
Tokenization and prompt failures do not mutate session state.
Backend failures invalidate only the affected in-flight outputs unless documented.
Run artifacts are execution-local evidence.
```

### CLI Doctrine

Implemented by:

```text
A0: CLI bootstrap
A0.1: command table and command metadata
B0: paths command
C0: inspect command
C1: metadata and tensors commands
E0: tokenize, detokenize, prompt commands
F0: graph and plan commands
I0: run and chat commands
L0: cuda-info command
```

Rules:

```text
YVEX is CLI-only.
Implemented commands are listed by yvex commands.
Future commands are documented only under their implementing wave.
stdout carries generated or machine-readable payloads.
stderr carries errors, logs, status, and diagnostics.
Unknown commands exit 2.
```

### YAI Boundary Doctrine

Implemented by:

```text
P0: boundary definition
K0: local provider surface when implemented
M0-M8: model support evidence consumed by YAI only after command proof
```

Rules:

```text
YVEX executes models.
YAI controls case, authority, governance, memory, tools, and records.
YVEX run receipts are execution-local until YAI imports them.
YVEX does not call YAI internally in the early architecture.
```

### Validation Doctrine

Implemented by:

```text
A0/A0.1: build and unit-test baseline
B0: filesystem tests and CLI proof
C0: artifact/GGUF header fixtures and inspect proof
C1 onward: each parser/runtime claim adds fixtures and commands
```

Rules:

```text
Baseline validation must not require model downloads, CUDA hardware, network access, server startup, or a YAI checkout.
make check is the repository gate.
make smoke proves implemented CLI behavior.
git diff --check must pass before commit.
Each support claim has a command and a test.
```

## 8. Validation and Handoff Rules

Required validation for every implementation wave:

```text
make clean
make check
make smoke
git diff --check
```

### Validation Matrix

Required validation layers:

```text
unit tests:
  status/error/log
  filesystem paths
  artifact/range checks
  GGUF header/probe
  future dtype byte formulas
  future tensor role classifier

golden fixtures:
  GGUF header fixture
  malformed GGUF fixture
  future tokenizer text -> ids
  future ids -> text/bytes
  future chat render -> prompt
  future graph dump
  future memory-plan JSON
  future CLI JSON/JSONL envelopes

malformed parser fixtures:
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

future validation layers:
  parser fuzzing
  address sanitizer
  undefined behavior sanitizer
  thread sanitizer where useful
  CUDA memcheck/sanitizer where available
  CPU/CUDA parity fixtures
  long-running decode soak tests
  server streaming conformance tests

manual proof commands:
  make clean
  make check
  make smoke
  git diff --check
```

Documentation-only gates must additionally prove:

```text
docs tree contains only the canonical docs
runtime code is unchanged
next authorized milestone is unchanged unless the gate explicitly changes it
```

Canonical docs tree:

```text
docs/README.md
docs/api.md
docs/backend-contract.md
docs/cli-runtime.md
docs/runtime-filesystem.md
docs/spine.md
```

Handoff packet for each completed wave:

```text
wave id
status
files created
files modified
public API added
CLI surface added
tests added
validation commands
validation result
known non-goals
next authorized milestone
```

Support claim rule:

```text
A support claim is valid only when the relevant code exists, tests pass,
and a command-visible proof exists.
```

Current handoff:

```text
from: DOC-GATE.2R
to: C1 - GGUF metadata and tensor directory
authorized work: metadata parser, tensor directory parser, fixtures, CLI metadata/tensors commands
blocked work: D0 and later until C1 acceptance passes
```
