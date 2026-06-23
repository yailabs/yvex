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
docs/cli-interface-spine.md
docs/cli-runtime.md
```

If a focused document conflicts with this spine, this spine wins until the
focused document is reconciled.

Focused CLI ownership:

```text
docs/cli-interface-spine.md owns CLI layout and interface doctrine.
docs/cli-commands.md owns the command index.
docs/cli-runtime.md owns detailed command behavior.
```

## 1. Current State

Current phase:

```text
after CLI.CONSOLE.0
```

Current implementation commit:

```text
current commit
```

Next authorized milestone:

```text
M3 - Materialized-weight engine attachment
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

Artifact / GGUF directory:
  include/yvex/artifact.h
  include/yvex/gguf.h
  src/artifact/artifact.c
  src/artifact/range.c
  src/formats/gguf.c

Tensor / model descriptor:
  include/yvex/dtype.h
  include/yvex/tensor.h
  include/yvex/model.h
  src/model/dtype.c
  src/model/role.c
  src/model/tensor_table.c
  src/model/descriptor.c

Tokenizer / prompt rendering:
  include/yvex/tokenizer.h
  include/yvex/prompt.h
  src/tokenizer/tokenizer.c
  src/tokenizer/vocab.c
  src/tokenizer/special.c
  src/tokenizer/encode.c
  src/tokenizer/decode.c
  src/tokenizer/prompt.c

Graph / planner:
  include/yvex/graph.h
  include/yvex/op.h
  include/yvex/planner.h
  include/yvex/memory_plan.h
  src/graph/graph.c
  src/graph/value.c
  src/graph/op.c
  src/graph/builder.c
  src/graph/shape.c
  src/graph/dump.c
  src/graph/planner.c
  src/graph/memory_plan.c

Backend:
  include/yvex/backend.h
  src/backend/backend.c
  src/backend/cpu_backend.c
  src/backend/cpu_tensor.c
  src/backend/cpu_ops.c
  backends/cuda/cuda_backend.c
  backends/cuda/cuda_tensor.c
  backends/cuda/cuda_ops.c
  backends/cuda/cuda_info.c
  backends/cuda/cuda_errors.c
  backends/cuda/cuda_internal.h

Engine / session runtime:
  include/yvex/engine.h
  include/yvex/session.h
  include/yvex/kv.h
  include/yvex/logits.h
  src/session/engine.c
  src/session/session.c
  src/session/state.c
  src/session/kv.c
  src/session/logits.c
  src/session/runtime_diagnostics.c

CLI runtime shell:
  src/chat/chat.c
  src/chat/repl.c
  src/chat/slash.c
  src/chat/run_command.c
  src/chat/status_line.c

Metrics / tracing:
  include/yvex/metrics.h
  include/yvex/trace.h
  include/yvex/profile.h
  src/metrics/metrics.c
  src/metrics/trace.c
  src/metrics/profile.c
  src/metrics/run_artifacts.c
  src/metrics/time.c
  src/metrics/json_writer.c

Server shell:
  include/yvex/server.h
  src/server/server.c
  src/server/http.c
  src/server/router.c
  src/server/handlers.c
  src/server/server_metrics.c
  server/yvexd.c

Open-weight source manifest:
  include/yvex/source_manifest.h
  src/tools/source_manifest.c
  src/tools/source_manifest_json.c
  src/tools/source_manifest_scan.c

Native weight inventory:
  include/yvex/native_weights.h
  src/tools/native_weights.c
  src/tools/safetensors.c
  src/tools/safetensors_json.c
  src/tools/native_weight_report.c

GGUF template validator:
  include/yvex/gguf_template.h
  src/tools/gguf_template.c
  src/tools/gguf_template_validate.c
  src/tools/gguf_template_compare.c
  src/tools/gguf_template_report.c

Controlled GGUF emitter:
  include/yvex/gguf_emit.h
  src/tools/gguf_emit.c
  src/tools/gguf_emit_metadata.c
  src/tools/gguf_emit_tensor.c
  src/tools/gguf_emit_report.c
  src/tools/gguf_emit_internal.h

Open-weight conversion bridge:
  include/yvex/conversion.h
  include/yvex/qtype_support.h
  src/tools/conversion.c
  src/tools/conversion_plan.c
  src/tools/conversion_emit.c
  src/tools/conversion_payload.c
  src/tools/conversion_report.c
  src/tools/qtype_support.c
  src/tools/adapters/qwen_adapter.c
  src/tools/quantizers/q8_0_quant.c

External quantization job bridge:
  include/yvex/quant_job.h
  src/tools/quant_job.c
  src/tools/quant_job_json.c
  src/tools/quant_job_report.c
  src/tools/quant_job_internal.h

Weight mapping adapter contract:
  include/yvex/weight_mapping.h
  src/tools/weight_mapping.c
  src/tools/weight_mapping_report.c
  src/tools/adapters/deepseek_adapter.c
  src/tools/adapters/qwen_adapter.c

Quantization policy manifest:
  include/yvex/quant_policy.h
  src/tools/quant_policy.c
  src/tools/quant_policy_json.c
  src/tools/quant_policy_validate.c
  src/tools/quant_policy_from_template.c
  src/tools/quant_policy_report.c

Calibration / imatrix manifest:
  include/yvex/imatrix.h
  src/tools/imatrix.c
  src/tools/imatrix_json.c
  src/tools/imatrix_validate.c
  src/tools/imatrix_report.c

CLI:
  cli/yvex_cli.c
  implemented commands: info, help, commands, version, paths, inspect, metadata, tensors, tokenizer, tokenize, detokenize, prompt, graph, plan, backend, cuda-info, engine, session, run, chat, source-manifest, native-weights, gguf-template, gguf-emit, tensor-map, qtype-support, convert, quant-job, quant-policy, imatrix
  implemented binaries: yvex, yvexd

Tests:
  tests/test_status.c
  tests/test_error.c
  tests/test_version.c
  tests/test_log.c
  tests/test_fs.c
  tests/test_artifact.c
  tests/test_gguf.c
  tests/test_dtype.c
  tests/test_tensor_table.c
  tests/test_model_descriptor.c
  tests/test_tokenizer.c
  tests/test_prompt.c
  tests/test_shape.c
  tests/test_graph.c
  tests/test_memory_plan.c
  tests/test_planner.c
  tests/test_backend_cpu.c
  tests/test_backend_ops.c
  tests/test_engine.c
  tests/test_session.c
  tests/test_kv.c
  tests/test_logits.c
  tests/test_runtime_diagnostics.c
  tests/test_chat_runtime.c
  tests/test_slash_commands.c
  tests/test_metrics.c
  tests/test_trace.c
  tests/test_profile.c
  tests/test_run_artifacts.c
  tests/test_http.c
  tests/test_server.c
  tests/test_weight_mapping.c
  tests/test_qtype_support.c
  tests/test_qwen_adapter.c
  tests/test_conversion_plan.c
  tests/test_conversion_payload.c
  tests/test_quant_job.c
  tests/test_quant_policy.c
  tests/test_imatrix.c
  tests/test_gguf_emit.c
  tests/test_gguf_template.c
  tests/test_deepseek_adapter.c
  tests/test_safetensors_header.c
  tests/test_native_weights.c
  tests/test_source_manifest.c
  tests/test_cli_gguf_template.sh
  tests/test_cli_gguf_emit.sh
  tests/test_cli_native_weights.sh
  tests/test_cli_source_manifest.sh
  tests/test_cli_tensor_map.sh
  tests/test_cli_convert.sh
  tests/test_cli_quant_job.sh
  tests/test_cli_quant_policy.sh
  tests/test_cli_imatrix.sh
  tests/test_cuda_info.c
  tests/test_cuda_tensor.c
  tests/test_cuda_ops.c
  tests/test_cuda_parity.c
  tests/test_cli_run.sh
  tests/test_cli_chat.sh
  tests/test_cli_metrics.sh
  tests/test_cli_server.sh
  tests/test_cli_cuda.sh
  tests/test_cli.sh
```

Not implemented:

```text
model support ladder execution
inference
generation endpoints
compatibility claims
```

Interface policy:

```text
YVEX is CLI-only.
The user-facing executable surfaces in the current repository are build/bin/yvex and build/bin/yvexd.
New interface surfaces require an explicit spine decision before implementation.
```

Implemented public surfaces:

```text
core version/status/error/log
filesystem paths/run directories
artifact byte view
GGUF header/probe
GGUF metadata/tensor directory
dtype/qtype registry
tensor table
model descriptor
tokenizer metadata/vocab table
fixture tokenizer encode/decode
prompt renderer
graph values/ops
shape inference helpers
missing-role diagnostics
estimate-only memory plan
plan object with backend labels
backend ABI
CPU reference backend
CPU tensor allocation/read/write/copy
CPU F32 embed op
```

Planned public surfaces:

```text
session
chat/run
metrics/tracing
server shell
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

### QA / Bench / Test Spine

YVEX QA is staged by implemented capability. A validation level may only claim
the behavior it can observe directly in code, command output, and saved
artifacts.

QA levels:

```text
QA Level 0 - Build and unit correctness:
  make check
  C unit tests
  parser, dtype, tensor, materialization invariants

QA Level 1 - CLI smoke correctness:
  make smoke
  yvex inspect, metadata, tensors, tokenizer, plan, backend, run, chat, materialize

QA Level 2 - CUDA provider-node correctness:
  make check-cuda
  cuda-info
  backend cuda availability
  CUDA tensor movement
  CPU/CUDA parity

QA Level 3 - External model materialization:
  explicit external model environment variables
  DeepSeek materialization commands
  no model files committed
  materialized, skipped, or failed status captured
  failure reason captured with a specific unsupported dtype, tensor, allocation, or parser cause

QA Level 4 - Provider shell correctness:
  yvexd health, metrics, and models endpoints
  generation_available false until inference exists
  unsupported generation responses remain explicit

QA Level 5 - Inference benchmarks:
  forbidden until prefill, decode, sampler, and logits validity exist
```

Allowed benchmark and QA measurements now:

```text
parse time
tensor directory scan time
materialization time
bytes materialized
backend allocated bytes
CUDA memory before and after
trace JSONL
profile JSON
clean failure reason
provider health and status fields
```

Forbidden benchmark and QA claims now:

```text
tokens/sec generation
TTFT
decode TPS
completion latency
model quality scores
OpenAI compatibility benchmarks
DeepSeek support beyond parser/materialization evidence
```

Provider-node validation rules:

```text
Provider-node tests must name the node class, backend, model path source, and command.
CUDA provider-node tests must include cuda-info or equivalent device proof.
External model tests must keep model paths outside the repository.
External model tests must not add GGUF, safetensors, bin, or generated model artifacts to git.
Failure-case captures are valid QA artifacts when the failure reason is specific and reproducible.
```

Benchmark policy:

```text
Benchmarks are not inference benchmarks before inference exists.
Materialization metrics measure loading, parsing, transfer, allocation, and failure handling only.
Quality benchmarks require valid logits and a fixed prompt/continuation set.
Throughput benchmarks require implemented prefill/decode and reproducible prompt files.
Claims must cite the command, node/backend, model path class, and saved trace/profile/report.
```

### Open Weight Intake / GGUF Toolchain Spine

GGUF is an operational container, not the source of truth for serious
open-weight support. For YVEX, the source of truth is official open weights plus
a source manifest and a reproducible conversion or quantization recipe.

Open Weight Intake pipeline:

```text
1. Source manifest:
   Hugging Face repo, revision, files, license, local path, download command, log paths, status

2. Native weight inventory:
   safetensors shards, tensor names, shapes, dtypes, byte sizes

3. Architecture adapter:
   model family mapping for llama, qwen, deepseek, gemma, phi, kimi, glm, and future families

4. Tensor role mapping:
   native tensor names -> YVEX/GGUF roles

5. GGUF template or metadata synthesis:
   tokenizer, metadata, tensor order, shapes, architecture keys

6. Quantization policy:
   qtype per tensor class, role, layer, and expert

7. Calibration / imatrix:
   optional dataset-derived importance weights

8. GGUF emission:
   metadata, tensor directory, aligned tensor payload

9. GGUF validation:
   inspect, metadata, tensor table, byte accounting, hash/provenance

10. Materialization:
    yvex materialize CPU/CUDA

11. Execution:
    only later, after graph executor, prefill, decode, sampler, and logits validity exist
```

Current DeepSeek V4 Flash intake paths:

```text
official HF source: deepseek-ai/DeepSeek-V4-Flash
local native weights path: ~/lab/models/hf/deepseek/DeepSeek-V4-Flash
download logs path: ~/lab/artifacts/download-logs
source manifest path: ~/lab/manifests/deepseek
repository policy: these paths stay outside git
```

Source-of-truth rule:

```text
DeepSeek V4 Flash starts from official HF weights and a recorded conversion or
quantization recipe. A GGUF is an emitted and validated artifact, not the source
of truth.
```

Toolchain boundary:

```text
YVEX runtime core:
  consumes GGUF/materialized weights
  executes backend, session, and provider paths
  stays small and deterministic

YVEX open-weight tools:
  may read official HF/native weights
  may convert and quantize
  may emit GGUF
  may use calibration/imatrix
  must record provenance

Do not mix conversion code into session/backend runtime.
DeepSeek-specific conversion code belongs in an adapter/tool, not generic runtime.
Generic intake must apply to open-weight families through adapters.
```

Open Weight Intake / GGUF Toolchain ladder:

```text
OWI.0 - External Layout Inventory and Open-Weight Pipeline Spine
OWI.1 - Source manifest and model provenance contract
OWI.2 - Safetensors/native weight inventory reader
OWI.3 - GGUF template contract and validator
OWI.4 - Tensor mapping/architecture adapter contract
OWI.5 - Quantization policy manifest
OWI.6 - Calibration/imatrix contract
OWI.7 - First YVEX-owned GGUF emission from controlled source
OWI.8 - Open Weight Conversion Bridge, Qwen-first / DeepSeek-ready
OWI.9 - DeepSeek V4 Flash GGUF Quantization and Emission Bridge
OWI.FINAL.0 - Full reference purge, official OWI validation, and GGUF closeout gate
```

OWI non-goals until their named wave:

```text
running arbitrary external quantization by default
downloading model weights
claiming generated DeepSeek GGUF without command proof
materializing DeepSeek
copying external code into YVEX
implementing safetensors parsing
implementing quantization kernels
claiming inference
claiming benchmark results
```

### Current Command Surface

Current implemented commands:

```text
commands
backend
chat
cuda-info
detokenize
engine
graph
gguf-template
help
info
inspect
materialize
metadata
native-weights
paths
plan
prompt
quant-job
run
session
source-manifest
tensors
tokenize
tokenizer
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
| C1 | complete | GGUF metadata and tensor directory |
| D0 | complete | Tensor and model layer |
| E0 | complete | Tokenizer and prompt rendering |
| F0 | complete | Graph and planner |
| G0 | complete | CPU reference backend |
| H0 | complete | Engine and session runtime |
| I0 | complete | CLI run/chat runtime |
| J0 | complete | Metrics and tracing |
| K0 | complete | yvexd server/provider |
| L0 | complete | CUDA/DGX Spark backend |
| M0 | complete | Fixture weight materialization |
| QA.BENCH.0 | complete | QA and benchmark spine |
| OWI.0 | complete | external layout inventory and open-weight pipeline spine |
| OWI.1 | complete | Source manifest and model provenance contract |
| OWI.2 | complete | Safetensors/native weight inventory reader |
| OWI.3 | complete | GGUF template contract and validator |
| OWI.4 | complete | Tensor mapping and architecture adapter contract |
| OWI.5 | complete | Quantization policy manifest |
| OWI.6 | complete | Calibration and imatrix contract |
| OWI.7 | complete | First YVEX-owned GGUF emission from controlled source |
| OWI.8 | complete | Open Weight Conversion Bridge, Qwen-first / DeepSeek-ready |
| OWI.9 | complete | DeepSeek V4 Flash GGUF Quantization and Emission Bridge |
| OWI.FINAL.0 | complete | Full reference purge, official OWI validation, and GGUF closeout gate |
| RUNTIME.KV.0 | complete | KV cache residency and benchmark policy |
| RUNTIME.KV.1 | planned | Static KV size estimator |
| RUNTIME.KV.2 | planned | CUDA KV allocation proof |
| RUNTIME.KV.3 | planned | GPU paged KV allocator skeleton |
| RUNTIME.KV.4 | planned | Host RAM spill and NVMe cold-cache experiments |
| RUNTIME.KV.5 | planned | KV quantization policy |
| M1 | complete | Real model conversion/materialization gate |
| ARTIFACT.NAMING.0 | complete | GGUF artifact naming contract and existing artifact rename |
| M2 | complete | DeepSeek real-model materialization hardening |
| CLI.PACKAGE.0 | complete | Repository root launcher and CLI packaging baseline |
| REPO.OPERATING.0 | complete | Agent operating handbook and model artifact cards |
| CLI.CONSOLE.0 | complete | Canonical REPL layout and line editing boundary |
| M3 | next | Materialized-weight engine attachment |
| M4 | paused | First executable fixture graph path |
| M5 | paused | First real-model partial graph execution |
| M6 | paused | Prefill runtime foundation |
| M7 | paused | Decode and logits runtime foundation |
| M8 | paused | First constrained generation path |

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
complete
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
complete
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
src/model/role.c
src/model/tensor_table.c
src/model/descriptor.c
tests/test_dtype.c
tests/test_tensor_table.c
tests/test_model_descriptor.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex inspect reports descriptor-only summary
yvex tensors reports role/dtype/known byte accounting
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
model descriptor is tested
inspect reports status: descriptor-only
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
complete
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
src/tokenizer/tokenizer.c
src/tokenizer/vocab.c
src/tokenizer/special.c
src/tokenizer/encode.c
src/tokenizer/decode.c
src/tokenizer/prompt.c
tests/test_tokenizer.c
tests/test_prompt.c
tests/fixtures/gguf/valid-tokenizer-simple.gguf
tests/fixtures/gguf/tokenizer-missing-tokens.gguf
tests/fixtures/gguf/tokenizer-bad-token-type-len.gguf
tests/fixtures/gguf/tokenizer-bad-score-len.gguf
tests/fixtures/gguf/tokenizer-bad-special-id.gguf
tests/fixtures/gguf/tokenizer-unsupported-arch.gguf
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex tokenizer
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
CLI smoke for tokenizer/tokenize/detokenize/prompt
```

Acceptance:

```text
golden tokenization tests pass
prompt render tests pass
unsupported real tokenizer algorithms fail clearly
HuggingFace tokenizer JSON execution is not claimed
no model execution claim
```

Handoff:

```text
F0 receives tokenizer/prompt shape inputs for graph and planning work.
```

### F0 - Graph and planner

Status:

```text
complete
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
missing-role diagnostics
estimate-only memory summary
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
tests/test_shape.c
tests/test_memory_plan.c
tests/test_planner.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
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
planner tests
unsupported op reporting tests
CLI smoke for graph and plan
```

Acceptance:

```text
graph dumps are deterministic
memory plans are deterministic
unsupported operations are reported precisely
execution_ready remains false
no execution claim
```

Handoff:

```text
G0 receives graph IR and planner output for CPU reference backend work.
```

### G0 - CPU reference backend

Status:

```text
complete
```

Owns:

```text
backend ABI first implementation
CPU tensor allocation
CPU tensor read/write
CPU reference kernels for minimal graph path
parity basis for future CUDA
backend capability reporting
minimal F32 embed op
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
tests/test_backend_ops.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/backend-contract.md
docs/spine.md
```

CLI surface:

```text
yvex backend cpu
yvex backend cuda
yvex plan reports CPU backend availability
```

Tests:

```text
CPU backend allocation tests
CPU copy/read/write tests
minimal CPU op tests
planner backend status tests
CLI backend smoke tests
```

Acceptance:

```text
CPU backend tests pass
backend failures return yvex_error
no CUDA claim
no broad inference claim
execution_ready remains false
```

Handoff:

```text
H0 receives an implemented backend ABI for engine/session runtime.
L0 later uses CPU behavior as parity reference.
```

### H0 - Engine and session runtime

Status:

```text
complete
```

Owns:

```text
yvex_engine
yvex_session
session state machine
KV cache skeleton
logits buffer ownership
unsupported prefill/decode API skeleton over implemented backend behavior
runtime diagnostics
engine/session CLI proof
```

Does not own:

```text
full chat UX
server process
CUDA backend
benchmark claims
run/chat generation
```

Creates / modifies:

```text
include/yvex/session.h
include/yvex/engine.h
include/yvex/kv.h
include/yvex/logits.h
src/session/
tests/test_engine.c
tests/test_session.c
tests/test_kv.c
tests/test_logits.c
tests/test_runtime_diagnostics.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvex engine <file>
yvex session <file> --backend cpu
yvex session <file> --backend cpu --text TEXT --accept-tokens
yvex session <file> --backend cuda reports unsupported
```

Tests:

```text
session lifecycle tests
state transition tests
KV ownership tests
logits availability tests
runtime diagnostics tests
CLI engine/session smoke tests
```

Acceptance:

```text
session lifecycle is tested
state failures are explicit
KV/logits unavailable status is explicit
engine/session commands are command-visible
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
complete
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
JSON run result envelope
```

Does not own:

```text
server process
new backend kernels
CUDA
compatibility claims
real generation
```

Creates / modifies:

```text
src/chat/
cli/yvex_cli.c
tests/test_chat_runtime.c
tests/test_slash_commands.c
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
chat REPL parser tests
non-interactive output separation tests
slash command tests
```

Acceptance:

```text
command proof exists
non-TTY behavior is tested
no fake generated output is emitted
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
complete
```

Owns:

```text
metrics collector
trace JSONL
profile report
run metrics files
phase timing counters for implemented runtime phases
accepted-token counters
```

Does not own:

```text
benchmark result claims
decode throughput
TTFT
generated-token counters
CUDA timing
server implementation
new backend behavior
```

Creates / modifies:

```text
include/yvex/metrics.h
include/yvex/trace.h
include/yvex/profile.h
src/metrics/
tests/test_metrics.c
tests/test_trace.c
tests/test_profile.c
tests/test_run_artifacts.c
cli/yvex_cli.c
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
--metrics-out
--trace-out
--profile-out
--save-run
--run-dir
```

Tests:

```text
metrics JSON tests
trace JSONL tests
profile output tests
run artifact path tests
run/chat CLI artifact smoke tests
```

Acceptance:

```text
JSON/JSONL output is valid
metrics are present for implemented runtime paths
no benchmark claim exists without reproducible artifacts
no decode/generation metrics are emitted
```

Handoff:

```text
K0 receives observability primitives for server shell behavior.
L0 receives measurement hooks for backend parity and timing.
```

### K0 - yvexd server/provider

Status:

```text
complete
```

Owns:

```text
yvexd process
HTTP server skeleton
health endpoint
metrics endpoint
local provider boundary
model catalog status shell
unsupported generation endpoint response
```

Does not own:

```text
YAI governance
YAI case state
new model execution behavior
completion/chat completion behavior
streamed generated output
compatibility claims
```

Creates / modifies:

```text
include/yvex/server.h
src/server/
server/yvexd.c
tests/test_http.c
tests/test_server.c
tests/test_cli_server.sh
Makefile
docs/api.md
docs/cli-runtime.md
docs/spine.md
```

CLI surface:

```text
yvexd --help
yvexd --version
yvexd --host 127.0.0.1 --port PORT --one-request
HTTP /health
HTTP /metrics
HTTP /v1/models
HTTP /v1/completions returns 501 unsupported
```

Tests:

```text
server help tests
health endpoint tests
metrics endpoint tests
model catalog endpoint tests
unsupported generation endpoint tests
localhost one-request smoke tests
```

Acceptance:

```text
health endpoint is tested
metrics endpoint is tested
model catalog status endpoint is tested
generation endpoint returns unsupported status
no generation or compatibility claim exists
```

Handoff:

```text
L0 receives server-observable status and metrics shell, but no generation provider path.
```

### L0 - CUDA/DGX Spark backend

Status:

```text
complete
```

Owns:

```text
CUDA backend attachment
cuda-info command
device memory stats
CUDA tensor allocation
CUDA tensor read/write/copy
CPU/CUDA parity fixtures
first CUDA scoped op: F32 embedding lookup
explicit CUDA build/test targets
```

Does not own:

```text
CUDA support claims before tests
model support ladder execution by itself
server shell behavior
CUDA matmul
CUDA RMSNorm
CUDA attention
GPU KV cache runtime
sampler
inference
```

Creates / modifies:

```text
backends/cuda/
tests/test_cuda_info.c
tests/test_cuda_tensor.c
tests/test_cuda_ops.c
tests/test_cuda_parity.c
tests/test_cli_cuda.sh
Makefile
cli/yvex_cli.c
docs/backend-contract.md
docs/spine.md
```

CLI surface:

```text
yvex cuda-info
yvex backend cuda
yvex plan <file> --backend cuda
```

Tests:

```text
cuda-info smoke tests
device memory stat tests
CPU/CUDA parity fixtures for each implemented op
CUDA failure mapping tests where practical
CLI CUDA smoke test
```

Acceptance:

```text
CUDA build target is explicit
first implemented CUDA op has CPU parity
device memory failures are explicit
no broad CUDA backend claim
normal make check remains CPU-safe
```

Handoff:

```text
Model support waves may use CUDA only for support levels backed by command proof.
```

### Model Support Ladder

The M ladder resumes after OWI.9 establishes the open-weight
provenance/toolchain path. The RUNTIME.KV map is explicit, but implementation
waves remain planned until a later runtime gate authorizes them.

Rules:

```text
Each model support wave has its own ownership, non-goals, and gate.
No M wave may use an arbitrary prebuilt GGUF as source of truth.
Every support level requires command proof.
Large-model feasibility is reported as feasibility until execution is proven.
No broad model support claim is valid without generated/staged artifact provenance.
```

### M1 - Real Model Conversion/Materialization Gate

Status:

```text
complete
```

Owns:

```text
Qwen selected materialization gate
DeepSeek selected materialization gate
provenance-controlled generated artifact paths
CPU and CUDA materialization proof where available
file identity and hash validation
expected tensor specification validation
model support-level classification
no model files committed
```

Does not own:

```text
full Qwen model conversion
full DeepSeek model conversion
full-model support claim
inference
graph execution
prefill/decode
sampler
server completions
```

Acceptance:

```text
Qwen selected GGUF passes model-gate check
DeepSeek selected GGUF passes model-gate check
CPU and CUDA materialization pass for both selected artifacts
support_level is selected-tensor-materialized
execution_ready remains false
no inference claim
```

Truth statement:

```text
M1 proves selected real-model materialization only.
M1 does not prove full-model support.
M1 does not prove graph execution.
M1 does not prove prefill or decode.
M1 does not prove inference.
```

### ARTIFACT.NAMING.0 - GGUF Artifact Naming Contract and Existing Artifact Rename

Status:

```text
complete
```

Owns:

```text
canonical generated GGUF artifact filename grammar
Qwen selected artifact rename
DeepSeek selected artifact rename
artifact naming helper API
external rename validation reports
M1 gate rerun on renamed paths
```

Rules:

```text
Generated real-model GGUF names must encode family, model, scope, artifact
class, qprofile, calibration, producer, and schema. Filename is a compact
identity. External reports and manifests carry full details.
```

Does not own:

```text
new conversion
new quantization
full-model GGUF emission
runtime KV
graph execution
prefill/decode
inference
```

### M2 - Real-Model Materialization Hardening

Status:

```text
complete
```

Owns:

```text
materialization failure cleanup
memory pressure behavior
unsupported dtype behavior
split GGUF policy
partial/full materialization reporting
external model regression scripts
```

M2 live target policy:

```text
DeepSeek V4 Flash selected GGUF is the only live external model target.
Qwen artifacts remain historical OWI/M1 evidence only.
M2 may use tiny fixtures for deterministic failure tests, but no Qwen live
gate is required or desired.
```

M2 proof:

```text
DeepSeek selected embedding materialize-gate passed
scope: selected-tensor
repeat_count: 3
cpu: pass
cuda: pass
cleanup_verified: yes
failure_class: none
execution_ready: false
```

Does not own:

```text
graph execution
inference
sampler
benchmark claims
```

Acceptance:

```text
materialization can fail without leaking allocations
summary/partial/failed statuses are reliable
CUDA memory before/after is sane
unsupported storage accounting is explicit
```

### CLI.PACKAGE.0 - Repository Root Launcher and CLI Packaging Baseline

Status:

```text
complete
```

Owns:

```text
root ./yvex launcher
root ./yvexd launcher
launcher smoke tests
repo-local vs global CLI documentation
packaging baseline posture
```

Does not own:

```text
compiled binary relocation
system packages
runtime behavior
model support
inference
```

Acceptance:

```text
./yvex commands works after build
./yvexd --help works after build
missing build output exits 127 with clear message
docs prefer ./yvex for repo-local examples
build/bin remains the compiled output
```

### REPO.OPERATING.0 - Agent Operating Handbook and Model Artifact Cards

Status:

```text
complete
```

Owns:

```text
AGENTS.md
MODEL_ARTIFACTS.md
repository operating contract
model artifact cards
active DeepSeek target declaration
Qwen historical-only declaration
support level posture
validation command map
```

Does not own:

```text
runtime behavior
conversion behavior
materialization behavior
server lifecycle behavior
inference
```

Acceptance:

```text
AGENTS.md exists
MODEL_ARTIFACTS.md exists
DeepSeek active target is explicit
Qwen historical-only status is explicit
support levels are accurate
no forbidden external reference/toolchain names
no full model or inference claim
```

### CLI.CONSOLE.0 - Canonical REPL Layout and Line Editing Boundary

Status:

```text
complete
```

Owns:

```text
CLI interface spine
REPL vs one-shot diagnostic distinction
prompt/run/chat role clarification
line editing boundary
future console implementation ladder
```

Does not own:

```text
code behavior change
REPL implementation
line editor dependency
inference
runtime execution
```

Creates / modifies:

```text
docs/cli-interface-spine.md
docs/spine.md
docs/cli-commands.md
docs/cli-runtime.md
docs/README.md
README.md
Makefile docs allowlist
```

Acceptance:

```text
CLI interface spine exists
./yvex interactive target is documented
one-shot diagnostics are separated from REPL
yvexd remains daemon/provider
line editor remains CLI-layer only
no code behavior changes
```

### M3 - Materialized-Weight Engine Attachment

Status:

```text
next
```

Owns:

```text
attaching materialized weights to yvex_engine
engine summary includes weight residency
session can see materialized weight status
no graph execution yet unless explicitly scoped
```

Does not own:

```text
prefill/decode
sampler
generation
```

Acceptance:

```text
engine can report descriptor plus materialized weight table
CPU/CUDA residency is visible
execution_ready remains false unless graph requirements are met later
```

### M4 - First Executable Fixture Graph Path

Status:

```text
paused
```

Owns:

```text
executing the existing fixture graph path over materialized weights
embed op through backend
activation output validation
CPU/CUDA parity for fixture graph
```

Does not own:

```text
real model execution
logits
sampler
decode loop
```

Acceptance:

```text
fixture graph produces expected hidden activation
CPU/CUDA parity passes
still no generated text
```

### M5 - First Real-Model Partial Graph Execution

Status:

```text
paused
```

Owns:

```text
first partial execution path on a real model component
selected supported op subset
explicit missing-op diagnostics
```

Does not own:

```text
full model run
decode
generation
```

Acceptance:

```text
selected real-model tensors can feed supported backend ops
unsupported graph segments are reported cleanly
```

### M6 - Prefill Runtime Foundation

Status:

```text
paused
```

Owns:

```text
prefill state machine
prompt token ingestion into executable runtime
KV allocation policy if required
logits precondition path
```

Does not own:

```text
sampler
open-ended generation
benchmark claims
```

Acceptance:

```text
prefill either executes a valid supported graph path or fails with precise missing op/runtime reason
no fake completion
```

### M7 - Decode and Logits Runtime Foundation

Status:

```text
paused
```

Owns:

```text
decode step structure
logits buffer lifecycle
next-token precondition
backend op readiness checks
```

Does not own:

```text
sampling policy
chat UX completion
benchmark claims
```

Acceptance:

```text
decode step is structurally implemented only where backend graph support exists
logits status is real
failure is explicit otherwise
```

### M8 - First Constrained Generation Path

Status:

```text
paused
```

Owns:

```text
first real generated-token path
minimal sampler
constrained prompt fixture
strict output/provenance reporting
```

Does not own:

```text
broad model support
benchmark marketing
OpenAI compatibility claim
```

Acceptance:

```text
one supported model/path can generate under strict constraints
output is marked experimental
benchmark gates remain separate under QA policy
```

### QA.BENCH.0 - QA and benchmark spine

Status:

```text
complete
```

Owns:

```text
external QA/bench/test read-only inventory
YVEX QA level taxonomy
allowed materialization/provider measurements
forbidden inference benchmark claims
provider-node validation policy
external-model validation policy
M ladder QA gates
```

Does not own:

```text
running external benchmarks
adding YVEX benchmark code
claiming inference performance
claiming model quality
```

Creates / modifies:

```text
docs/spine.md
```

Acceptance:

```text
QA levels are canonical in the spine
allowed and forbidden metrics are explicit
DeepSeek materialization gates are explicit
no new docs sprawl
no benchmark claims before inference exists
```

### OWI.0 - External Layout Inventory and Open-Weight Pipeline Spine

Status:

```text
complete
```

Owns:

```text
read-only external layout inspection
external GGUF layout inventory
DeepSeek quantizer structure analysis
GGUF template role summary
imatrix/calibration role summary
generic open-weight pipeline
runtime/toolchain boundary
OWI ladder insertion into the spine
M1 pause/resume rule
```

Does not own:

```text
running DeepSeek quantization
downloading model weights
generating a DeepSeek GGUF
materializing DeepSeek
modifying external reference repositories
copying external source into YVEX
implementing safetensors parser
implementing quantization kernels
inference
benchmark claims
```

Creates / modifies:

```text
docs/spine.md
```

Acceptance:

```text
external layout paths inspected read-only
docs/spine.md updated with OWI pipeline
OWI.1 through OWI.8 expanded as explicit future waves
M1 marked paused pending OWI path
no new docs files unless explicitly approved
no model files are committed
validation passes
```

Handoff:

```text
next wave is OWI.1 - Source manifest and model provenance contract (complete)
```

### OWI.1 - Source Manifest and Model Provenance Contract

Status:

```text
complete
```

Owns:

```text
official model source manifest schema
Hugging Face repo identity
model revision / commit hash
license reference
expected file list
local provider-node path
download command record
dry-run/download log references
optional checksum manifest
source/provenance status vocabulary
local source directory scan
local file manifest writer
CLI source-manifest create command
```

Does not own:

```text
safetensors parsing
quantization
GGUF emission
materialization
model inference
modifying official weights
committing model files
```

Expected files:

```text
include/yvex/source_manifest.h
src/tools/source_manifest.c
src/tools/source_manifest_json.c
src/tools/source_manifest_scan.c
src/tools/source_manifest_internal.h
tests/test_source_manifest.c
tests/test_cli_source_manifest.sh
```

Implemented CLI/tool surface:

```text
yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE
```

Acceptance:

```text
source manifest can describe an official model source
local path is outside repo
model weights are not committed
manifest distinguishes official source from generated GGUF
manifest can record download command/log path
manifest can record in-progress downloads
manifest lists relative local file paths when requested
baseline tests use fake local source trees only
validation passes without requiring the full model download in baseline
```

Handoff:

```text
OWI.2 can inventory native weight files using this provenance record
```

### OWI.2 - Safetensors / Native Weight Inventory Reader

Status:

```text
complete
```

Owns:

```text
native weight file inventory
safetensors shard discovery
tensor metadata reading
tensor name/shape/dtype/byte-size inventory
shard-to-tensor mapping
native model storage summary
read-only source validation
safetensors header length parsing
targeted safetensors JSON parsing
native-weights CLI inventory command
```

Does not own:

```text
converting tensors
quantization
GGUF writing
tokenizer conversion
model execution
graph execution
full tensor payload loading unless needed for metadata proof
```

Expected files:

```text
include/yvex/native_weights.h
src/tools/native_weights.c
src/tools/safetensors.c
src/tools/safetensors_json.c
src/tools/native_weight_report.c
src/tools/native_weights_internal.h
tests/test_safetensors_header.c
tests/test_native_weights.c
tests/test_cli_native_weights.sh
```

Acceptance:

```text
can list safetensors shards for a local model path
can read tensor names/shapes/dtypes from safetensors metadata
can report total native tensor count and known bytes
can fail cleanly on missing/incomplete shards
does not load full model into memory
does not require inference
native-weights-empty is valid when no completed safetensors files exist yet
```

Handoff:

```text
OWI.3 can compare native inventory against GGUF template requirements
```

### OWI.3 - GGUF Template Contract and Validator

Status:

```text
complete
```

Owns:

```text
GGUF template concept
template metadata validation
tokenizer metadata validation
architecture key validation
tensor order/shape contract
template/native inventory compatibility checks
template provenance record
template status and issue vocabulary
gguf-template CLI inspect, validate, and compare
```

Does not own:

```text
quantization
emitting final GGUF
safetensors payload conversion
model execution
tokenizer implementation beyond metadata validation
```

Expected files:

```text
include/yvex/gguf_template.h
src/tools/gguf_template.c
src/tools/gguf_template_validate.c
src/tools/gguf_template_compare.c
src/tools/gguf_template_report.c
src/tools/gguf_template_internal.h
tests/test_gguf_template.c
tests/test_cli_gguf_template.sh
```

Implemented CLI/tool surface:

```text
yvex gguf-template inspect --template FILE
yvex gguf-template validate --template FILE
yvex gguf-template compare --template FILE --native-source DIR
```

Acceptance:

```text
can inspect a template GGUF
can report required metadata/tokenizer/tensor-order fields
can compare template tensor expectations with native inventory
fails clearly when template and native weights disagree
does not claim final GGUF generation
exact-name native comparison only; architecture mapping belongs to OWI.4
```

Handoff:

```text
OWI.4 can define architecture-specific tensor mapping rules
```

### OWI.4 - Tensor Mapping and Architecture Adapter Contract

Status:

```text
complete
```

Owns:

```text
native tensor name mapping
architecture adapter boundary
DeepSeek adapter contract
generic adapter interface for llama/qwen/deepseek/gemma/phi/kimi/glm
native tensor role classification
GGUF tensor name target mapping
MoE/expert tensor grouping rules
```

Does not own:

```text
quantization
tensor payload conversion
GGUF emission
inference
backend execution
```

Expected files:

```text
include/yvex/weight_mapping.h
src/tools/weight_mapping.c
src/tools/weight_mapping_report.c
src/tools/adapters/deepseek_adapter.c
tests/test_weight_mapping.c
tests/test_deepseek_adapter.c
tests/test_cli_tensor_map.sh
```

Acceptance:

```text
can map known native tensor names to YVEX/GGUF roles
can classify DeepSeek MoE/expert tensors structurally
can report unmapped tensors
can fail cleanly on unknown architecture
mapping is metadata/contract only, not execution
DeepSeek embed.weight maps to token_embedding -> token_embd.weight with transpose shape compatibility
```

Handoff:

```text
OWI.5 can assign quantization policies by tensor role/class
```

### OWI.5 - Quantization Policy Manifest

Status:

```text
complete
```

Owns:

```text
quantization policy schema
qtype selection by tensor role/class/layer/expert
policy validation
supported/unsupported qtype declaration
storage-accounting compatibility
compute-support separation
```

Does not own:

```text
implementing quantization kernels
running calibration
emitting GGUF
CUDA execution
inference benchmarks
```

Expected files:

```text
include/yvex/quant_policy.h
src/tools/quant_policy.c
src/tools/quant_policy_json.c
src/tools/quant_policy_validate.c
src/tools/quant_policy_from_template.c
src/tools/quant_policy_report.c
tests/test_quant_policy.c
tests/test_cli_quant_policy.sh
```

Policy concepts:

```text
embedding tensors
attention projection tensors
MLP tensors
MoE router tensors
MoE expert tensors
output/norm tensors
small sensitive tensors
```

Acceptance:

```text
can parse/validate a quantization policy
can report qtypes per tensor class
separates storage materialization support from compute support
rejects unknown qtypes
records why a tensor is Q8_0/Q4_K/Q2_K/IQ2_XXS/etc.
does not claim quantized inference
derives a policy manifest from GGUF template tensor qtype distribution
validates storage support separately from compute support
exposes yvex quant-policy inspect/validate/derive
```

Handoff:

```text
OWI.6 can attach calibration/imatrix data to the policy
```

### OWI.6 - Calibration and Imatrix Contract

Status:

```text
complete
```

Owns:

```text
calibration dataset reference contract
imatrix file role
imatrix provenance
tensor/column/expert importance mapping contract
validation that a quantization policy expects/uses imatrix
imatrix concept mapping into YVEX vocabulary
```

Does not own:

```text
generating imatrix unless explicitly scoped
running model inference for calibration
quantization kernels
GGUF emission
benchmark claims
```

Expected files:

```text
include/yvex/imatrix.h
src/tools/imatrix.c
src/tools/imatrix_json.c
src/tools/imatrix_validate.c
src/tools/imatrix_report.c
tests/test_imatrix.c
tests/test_cli_imatrix.sh
```

Acceptance:

```text
can describe an imatrix artifact
can associate imatrix with model/source manifest
can validate presence/shape/metadata if format is known
can declare imatrix missing/unsupported cleanly
does not require calibration run in baseline
exposes yvex imatrix create/inspect/validate
checks policy rules that declare requires_imatrix=true
references external .dat artifacts without copying them
```

Handoff:

```text
OWI.7 can use source/template/mapping/policy/imatrix contracts to emit a controlled GGUF
```

### OWI.7 - First YVEX-Owned GGUF Emission From Controlled Source

Status:

```text
complete
```

Owns:

```text
first YVEX-owned GGUF emission path
controlled source input
metadata writing
tensor directory writing
alignment/payload writing
generated GGUF validation through existing C1/D0/M0 stack
fixture/native-small source only
```

Does not own:

```text
DeepSeek full conversion yet
huge model conversion
full quantization suite
imatrix generation
inference
server generation
```

Expected files:

```text
include/yvex/gguf_emit.h
src/tools/gguf_emit.c
src/tools/gguf_emit_metadata.c
src/tools/gguf_emit_tensor.c
src/tools/gguf_emit_report.c
src/tools/gguf_emit_internal.h
tests/test_gguf_emit.c
tests/test_cli_gguf_emit.sh
```

Acceptance:

```text
emits a GGUF from a controlled tiny/native source
generated GGUF passes yvex inspect
generated GGUF passes yvex metadata
generated GGUF passes yvex tensors
generated GGUF passes yvex materialize --backend cpu
generated GGUF passes yvex materialize --backend cuda when available
output GGUF is test fixture sized only
no large model committed
```

Handoff:

```text
OWI.8 adapts the emission/conversion bridge to Qwen-first and DeepSeek-ready open weights
```

### OWI.8 - Open Weight Conversion Bridge, Qwen-First / DeepSeek-Ready

Status:

```text
complete
```

Owns:

```text
open-weight conversion bridge
Qwen3 selected tensor conversion path
DeepSeek conversion plan path
official HF source manifest usage
native safetensors inventory usage
architecture adapter usage
reference-layout template/quantization/imatrix bridge
YVEX-owned selected tensor GGUF generation path
qtype support matrix
```

Does not own:

```text
generic inference
server completions
benchmark claims
copying external code blindly
committing external weights/GGUF
OpenAI compatibility
```

Expected behavior:

```text
official HF weights
-> YVEX source manifest
-> native inventory
-> architecture adapter mapping
-> template/metadata contract
-> quantization policy
-> optional imatrix
-> GGUF emission/conversion
-> GGUF validation
-> YVEX materialization
```

Acceptance:

```text
conversion bridge has explicit provenance
selected-tensor generated GGUF is validated by YVEX
materialization works or fails cleanly with specific reason
no inference claim
no generated output claim
no model files committed
```

Handoff:

```text
OWI.9 closes Open Weight Intake with DeepSeek GGUF production/staging provenance
```

### OWI.9 - DeepSeek V4 Flash GGUF Quantization and Emission Bridge

Status:

```text
complete
```

Owns:

```text
DeepSeek external quantization job manifest
generic external quantization job discovery proof
DeepSeek GGUF output path convention
quantization command provenance
template path provenance
imatrix path/status provenance
YVEX quant-job create/inspect/validate CLI
clean missing-output handling for non-succeeded jobs
clean missing-imatrix reporting
```

Does not own:

```text
native YVEX Q2_K/IQ2_XXS quantization suite
replacing an external quantizer
copying external code into YVEX runtime
DeepSeek inference
prefill/decode
sampler
KV cache implementation
server completions
benchmark claims
committing generated DeepSeek GGUF
committing DeepSeek safetensors
committing imatrix .dat files
```

Handoff:

```text
M1 resumes with a provenance-controlled DeepSeek quantization job bridge and
clear generated-artifact path, whether the external GGUF exists or is blocked
by missing imatrix/output diagnostics.
```

### OWI.FINAL.0 - Full Reference Purge, Official OWI Validation, and GGUF Closeout Gate

Status:

```text
complete
```

Owns:

```text
full tracked-file forbidden-reference purge
generic quant-job provenance contract
official OWI command surface proof
Qwen produced GGUF validation
DeepSeek selected GGUF validation or precise unsupported reason
full DeepSeek GGUF status declaration
external OWI closeout report
GO/NO-GO M1 gate
```

Does not own:

```text
native full DeepSeek Q2_K quantization
native full DeepSeek IQ2_XXS quantization
native full DeepSeek Q4_K quantization
native imatrix generation
external tool execution
runtime KV implementation
prefill
decode
sampler
inference
server completions
benchmark claims
committing generated model artifacts
```

Reference-layout boundary:

```text
YVEX may consult external reference layouts, tensor order, metadata structure,
qtype distributions, and calibration concepts while designing its own
open-weight toolchain.

YVEX must not depend on external repository binaries, external quantizer
binaries, or external toolchains as the canonical production path.

YVEX-owned artifacts are produced by YVEX commands and validated by YVEX
commands. External artifacts may be compared for research/debugging only.
```

OWI final status rule:

```text
OWI is closed only when:
  tracked reference/toolchain names are purged
  YVEX command surface is verified
  at least one YVEX-produced real-model GGUF artifact is inspected
  Qwen selected tensor GGUF is materialized on CPU and CUDA
  DeepSeek selected tensor GGUF is either produced and checked or cleanly reported unsupported
  full DeepSeek GGUF status is explicit
  generated artifacts remain outside repo
```

Handoff:

```text
M1 can begin from the official OWI closeout report and generated-artifact gates.
```

### RUNTIME.KV.0 - KV Cache Residency and Benchmark Policy

Status:

```text
complete
```

Owns:

```text
KV cache sizing formula
KV residency tiers
hot/cold KV vocabulary
future KV status vocabulary
benchmark policy for KV work
future KV experiment ladder
M6/M7 dependency notes
```

Does not own:

```text
KV cache implementation
CUDA KV allocation
PagedAttention or vAttention
CPU/RAM spill implementation
NVMe offload implementation
KV quantization
attention kernels
prefill
decode
logits
sampler
inference benchmark claims
new public C API
```

Docs-only statement:

```text
RUNTIME.KV.0 is a spine policy wave. It defines runtime KV vocabulary and
benchmark guardrails but does not allocate, page, spill, quantize, or execute KV.
```

CLI surface:

```text
none
```

Acceptance:

```text
KV cache remains unavailable in runtime output
future KV waves are explicit in the delivery map
no allocation or decode claim exists
```

Handoff:

```text
RUNTIME.KV.1 can add a static estimator without changing runtime residency.
```

### RUNTIME.KV.1 - Static KV Size Estimator

Status:

```text
planned
```

Owns:

```text
static KV memory estimator
model metadata to KV sizing inputs
context length and batch/session count inputs
GQA/MQA-aware kv_head_count accounting
dtype/bytes-per-element policy
CLI estimator proof
```

Does not own:

```text
KV allocation
CUDA memory allocation
attention kernels
prefill
decode
runtime cache residency
performance claims
```

Expected future files:

```text
include/yvex/kv_estimator.h
src/session/kv_estimator.c
tests/test_kv_estimator.c
```

Future CLI surface:

```text
yvex kv estimate --model FILE --context N --batch N --dtype f16
```

Acceptance:

```text
estimates KV bytes deterministically
reports required fields and unknown fields
no allocation occurs
no decode benchmark claim
no inference claim
```

Handoff:

```text
RUNTIME.KV.2 can use estimator output to allocate CUDA KV buffers.
```

### RUNTIME.KV.2 - CUDA KV Allocation Proof

Status:

```text
planned
```

Owns:

```text
CUDA KV buffer allocation proof
allocation/free lifecycle
CUDA memory before/after reporting
allocator failure diagnostics
CPU-safe fallback behavior
```

Does not own:

```text
attention kernels
PagedAttention
decode
prefill
KV paging
host spill
inference
```

Expected future files:

```text
src/session/kv_alloc.c
tests/test_kv_alloc.c
tests/test_cuda_kv_alloc.c
```

Future CLI surface:

```text
yvex kv allocate --backend cuda --layers N --context N --kv-heads N --head-dim N --dtype f16
```

Acceptance:

```text
CUDA KV allocation succeeds or fails cleanly
no memory leak after free
memory before/after is reported
no decode claim
no inference claim
```

Handoff:

```text
RUNTIME.KV.3 can introduce paged/block allocation.
```

### RUNTIME.KV.3 - GPU Paged KV Allocator Skeleton

Status:

```text
planned
```

Owns:

```text
GPU KV page/block table
fixed-size block allocation
free-list accounting
sequence-to-block mapping
fragmentation reporting
allocator simulation and optional CUDA allocation proof
```

Does not own:

```text
attention kernel integration
PagedAttention performance claim
vAttention performance claim
host spill
NVMe offload
decode benchmark
```

Expected future files:

```text
include/yvex/kv_paged.h
src/session/kv_paged.c
tests/test_kv_paged.c
```

Future CLI surface:

```text
yvex kv pages --simulate --context N --block-size N --sequences N
```

Acceptance:

```text
page table is deterministic
fragmentation report is deterministic
no attention execution
no performance claim
```

Handoff:

```text
M6/M7 can later decide whether paged KV becomes the active runtime path.
```

### RUNTIME.KV.4 - Host RAM Spill and NVMe Cold-Cache Experiments

Status:

```text
planned
```

Owns:

```text
host RAM spill experiment contract
pinned host memory copy measurement
NVMe cold-cache experiment contract
sequential/random read-write measurements
no-hot-decode policy enforcement
```

Does not own:

```text
enabling RAM spill by default
enabling NVMe decode
claiming SSD decode viability
attention kernels
prefill/decode
inference
```

Expected future files:

```text
src/session/kv_spill.c
tests/test_kv_spill.c
tests/test_kv_cold_cache.c
```

Future CLI surface:

```text
yvex kv spill-test --bytes N --backend cuda
yvex kv cold-cache-test --path PATH --bytes N
```

Acceptance:

```text
reports copy/read/write measurements as experiments only
does not claim decode performance
does not enable hot SSD KV
output clearly says experimental
```

Handoff:

```text
future runtime work may use results to decide if spill tiers are worth implementing.
```

### RUNTIME.KV.5 - KV Quantization Policy

Status:

```text
planned
```

Owns:

```text
KV dtype/quantization policy
K/V precision vocabulary
possible asymmetric K/V policy
quality-risk notes
relationship to model qtype policy
future calibration dependency
```

Does not own:

```text
implementing KIVI-like quantization
changing model weight quantization
attention kernel support
decode
inference quality claims
benchmark claims
```

Expected future files:

```text
include/yvex/kv_quant_policy.h
src/session/kv_quant_policy.c
tests/test_kv_quant_policy.c
```

Future CLI surface:

```text
yvex kv policy inspect
yvex kv policy estimate
```

Acceptance:

```text
policy can describe KV precision choices
separates storage/residency/compute support
no quality claim without evaluation
no decode benchmark claim
```

Handoff:

```text
future decode/runtime waves may consume KV quantization policy only after
attention kernels exist.
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
| C1 | 0de97e4 | Parsed GGUF metadata and tensor directory, added metadata/tensors CLI, fixtures, and tests. |
| D0 | dd2eb59 | Added dtype/qtype registry, tensor table, role classifier, descriptor-only model summary, CLI output, and tests. |
| E0 | 64be1b1 | Added tokenizer metadata/vocab table, fixture encode/decode, prompt rendering, CLI proof, fixtures, and tests. |
| F0 | 5234420 | Added graph values/ops, shape inference, missing-role diagnostics, estimate-only memory plans, graph/plan CLI proof, and tests. |
| G0 | 8d60c2b | Added backend ABI, CPU reference backend, CPU tensor allocation/read/write/copy, capability reporting, embed op proof, backend CLI, and tests. |
| H0 | e009e08 | Added engine/session runtime objects, KV/logits availability skeletons, session state machine, token acceptance diagnostics, engine/session CLI, and tests. |
| I0 | 425fab3 | Added accepted-only run/chat runtime shell, slash commands, JSON run output, piped chat tests, and CLI smoke coverage. |
| J0 | 5986659 | Added runtime metrics collector, trace JSONL writer, profile JSON writer, run artifacts, run/chat instrumentation, and tests. |
| K0 | 6a8e17b | Added yvexd server shell, HTTP status router, health/metrics/model catalog endpoints, unsupported generation endpoint response, and tests. |
| L0 | afc8536 | Added CUDA backend attachment, device probe, tensor allocation/read/write/copy, F32 embed op, CPU/CUDA parity proof, cuda-info CLI, and CUDA targets/tests. |
| M0 | current commit | Added fixture weight materialization into backend tensors, CPU/CUDA materialization proof, materialized weight table API, materialize CLI, and tests. |
| OWI.9 | current commit | Added external DeepSeek quantization job bridge, generic external tool discovery proof, command provenance, and quant-job CLI/tests. |
| OWI.FINAL.0 | current commit | Purged tracked external reference dependency names, validated OWI command surface and YVEX-produced GGUF artifacts, and wrote external closeout report. |

Current implemented CLI command set:

```text
commands
backend
chat
cuda-info
engine
session
detokenize
graph
gguf-emit
help
info
inspect
imatrix
materialize
metadata
native-weights
paths
prompt
plan
quant-job
quant-policy
qtype-support
run
source-manifest
tensor-map
tokenize
tokenizer
tensors
version
```

Current implemented binary set:

```text
yvex
yvexd
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
M3 - Materialized-weight engine attachment
```

Current active milestone:

```text
M3 - Materialized-weight engine attachment
```

Planned runtime KV map:

```text
RUNTIME.KV.0 complete
RUNTIME.KV.1 planned
RUNTIME.KV.2 planned
RUNTIME.KV.3 planned
RUNTIME.KV.4 planned
RUNTIME.KV.5 planned
```

M1 resumes after OWI.9. The official source/provenance contract, native
inventory, GGUF template contract, tensor mapping adapter, quantization policy
manifest, imatrix manifest contract, controlled GGUF emission proof, qtype
support matrix, selected-tensor conversion bridge, and DeepSeek quantization job
bridge now exist.

RUNTIME.KV implementation waves remain planned. They are not active while M1
starts the real model conversion/materialization gate.

Model support waves must produce:

```text
model support levels backed by command proof
fixture-first inspection/tokenization/execution steps
explicit support/unsupported state per model family
```

Model support waves must not produce:

```text
broad inference claim
model support claim without command proof
```

Required M1 entry proof:

```text
OWI.0 complete
OWI.1 provenance/source manifest direction clear
make clean
make check
make smoke
make check-cuda where CUDA is available
no broad inference claim
no random prebuilt GGUF source-of-truth claim
```

M ladder QA gates:

```text
M1 DeepSeek direct materialization gate:
  Spark node only for DeepSeek-scale CUDA proof
  DEEPSEEK_GGUF is an external path
  inspect passes or fails diagnostically
  cuda materialize direct command produces weights-materialized or weights-failed
  failure reason is specific
  no model file is committed
  execution_ready remains false

M2 materialization hardening gate:
  selected/full materialization policy
  memory cleanup proof
  unsupported dtype accounting proof
  split GGUF policy proof

Future executable-wave benchmark gate:
  prefill implemented
  decode implemented
  sampler implemented
  logits valid
  reproducible prompt set
  trace/profile/report artifacts saved
```

## 6. Planned Delivery Catalogue

The planned delivery catalogue is linear:

```text
H0 consumes G0/F0/E0 and creates engine/session runtime.
I0 consumes H0 and creates user-facing run/chat commands.
J0 consumes H0/I0 and creates metrics/tracing.
K0 consumes H0/I0/J0 and creates the yvexd server shell.
L0 consumes G0/J0 and creates CUDA/DGX Spark backend path.
M0 and later explicit M waves consume the implemented runtime/backends and assign model support levels.
```

Each planned wave is defined in Section 3. A wave that is not defined there is
not authorized.

Do-not-proceed gates:

```text
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
M waves: model-specific support levels
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
G0: backend command and CPU backend status in plan
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
K0: local server shell when implemented
M waves: model support evidence consumed by YAI only after command proof
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
C1: metadata/tensor directory fixtures and metadata/tensors command proof
D0 onward: each parser/runtime claim adds fixtures and commands
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
from: L0 - CUDA/DGX Spark backend
to: M0 - Fixture weight materialization, then explicit paused M waves
authorized work: fixture-first model support ladder with command proof
blocked work: broad model support, inference, and generation claims without actual execution proof
```
