# YVEX Runtime Contract

This document defines the observable contract of the YVEX runtime.

It covers command behavior, filesystem state, model reference resolution,
backend behavior, runtime ownership, graph admission, artifact integrity, output
rules, validation gates, and public claim rules. It is not an API reference and
not a project overview. The API surface is documented in [docs/api.md](api.md);
the public project story lives in [README.md](../README.md); operator command
transcripts live in [docs/operator-runbook.md](operator-runbook.md).

The contract is simple: every public runtime surface must say what state exists,
who owns it, which command created it, which checks ran before it, and where
execution stopped when a boundary refuses to advance.

## Scope

YVEX is a CLI-first local runtime for open-weight model artifacts.

The repository provides:

```text
./yvex   command-line runtime and diagnostics tool
./yvexd  server/provider status daemon
include/yvex/ public C headers
docs/    public and internal documentation
tests/   fixtures, vectors, and regression coverage
```

The runtime is organized around a staged local inference path:

```text
artifact
  -> descriptor
  -> backend residency
  -> engine attachment
  -> session visibility
  -> graph execution
  -> token input
  -> prefill state
  -> KV
  -> decode
  -> logits
  -> sampling
  -> generation
  -> server/provider surface
```

Each stage has its own command behavior, report shape, ownership rules, and
failure mode. YVEX must keep those stages distinguishable.

## Command Contract

`make` builds the root binaries:

```sh
make
```

The expected root binaries are:

```text
./yvex
./yvexd
```

`./yvex` is the main CLI. `./yvexd` is the daemon/status surface.

The command source of truth is:

```sh
./yvex commands
./yvex help
./yvex help <command>
```

Command help should show normal operator paths before advanced diagnostic flags.
Long exact flags belong to gates, CI checks, provenance, debug output, and
regression commands. A normal operator should be able to discover the short path
without reading the internal spine.

Commands that accept model references may accept either a filesystem path or a
registered alias where model selection is supported. Resolution follows this
order:

```text
existing filesystem path
registered model alias
clear not-found error with a registry hint
```

A command must never silently select an arbitrary model. `chat` may use the
current selected registry model when `--model` is omitted. If no current model is
selected, the command must fail with a useful operator hint. `yvexd --model
FILE_OR_ALIAS` resolves only the explicit filesystem path or alias supplied by
the operator.

## Filesystem Contract

Generated and local runtime state stays out of source control.

```text
build/      generated build and test output
.yvex/      ignored local operator state
./yvex      generated root binary
./yvexd     generated root binary
```

Model artifacts stay outside the repository. Local registries may contain
absolute paths because they are machine-local operator state. They must remain
untracked.

Tracked GGUF files are reserved for tiny fixtures under `tests/`. Real model
weights, generated model artifacts, local reports, local logs, cache files, and
machine-specific registries stay outside git.

The repository may contain compact fixtures, structural corruption fixtures,
small vectors, and regression data when they are small enough to be source
artifacts rather than model artifacts.

## Model Registry Contract

The default local model registry is:

```text
.yvex/models.local.json
```

It can be overridden with:

```text
YVEX_MODELS_REGISTRY
```

Registry aliases are local convenience references. They resolve to external
artifact paths. A registry entry may record file identity, support level,
format, architecture, tensor count, known tensor bytes, primary tensor facts,
and selected readiness facts.

Alias identity is local evidence. It helps the runtime detect drift between the
artifact originally registered and the artifact currently present at the same
path. It is not global provenance and does not turn a local alias into a trusted
distribution channel.

Safety-critical alias paths must verify identity before reading tensor payloads
for materialization, engine/session attachment, gate checks, or graph execution.
When digest or metadata drift invalidates the alias assumptions, the command
stops before backend allocation or graph dispatch.

Raw-path operations use structural integrity checks and explicit expected digest
checks when provided. They do not require registry metadata.

## Backend Contract

CPU is the reference and diagnostics backend. CUDA is the current acceleration
backend for implemented selected materialization and selected graph paths.

CUDA implementation lives under `cuda/`. CUDA host bridge code stays in C
translation units. CUDA device kernels live in `.cu` translation units. CUDA
availability is optional for baseline validation and required for CUDA
validation.

Backend failures must remain distinguishable. A caller or operator should be
able to tell whether failure came from backend discovery, allocation, transfer,
operation support, dispatch, output readback, or cleanup.

Materialization is the residency boundary. It moves selected tensor bytes from
artifact storage into backend-owned runtime storage. It proves allocation,
transfer, byte accounting, and cleanup behavior for selected tensors. Graph
execution begins after that boundary.

Engine attachment is the ownership boundary above materialization. The engine
owns attached selected materialized weights. Sessions may observe engine state
while the engine owns the attached weights. Engine close releases
engine-attached state.

## Runtime State Contract

Runtime state must have visible ownership.

Artifact handles own parsed artifact state. Backend tensors own backend memory.
Weight tables own selected backend-resident weights until moved under engine
ownership. Engines own attached weights. Sessions observe or own session-level
state depending on the runtime layer. Minimal KV state is session-owned. Future
KV-backed prefill, decode, logits, sampling, and generation objects must follow
the same pattern: the owner of the memory owns the cleanup path.

The implemented runtime states are:

| Runtime state | Current contract |
| --- | --- |
| Artifact facts | Parsed from local artifact paths and reported through GGUF/model/tensor surfaces. |
| Descriptor state | Built from parsed artifact facts and selected tensor roles. |
| Backend residency | Created by selected materialization on CPU or CUDA. |
| Engine attachment | Selected backend-resident weights attached to engine-owned state. |
| Session visibility | Sessions can observe engine attachment summaries. |
| Controlled graph result | Deterministic fixture graph execution over tiny F32 fixtures. |
| Selected graph result | Real selected embedding execution over F16 `token_embd.weight`. |
| Selected segment result | Embedding-plus-RMSNorm execution over selected real tensors. |
| Token input | Bounded explicit token sequences with validation and token-index selection. |
| Prefill state summary | Segment-summary state over validated explicit token input. |
| Minimal KV state | Session-owned F32 KV storage with shape, append/read, clear, overflow, and cleanup reports. |
| Minimal KV-backed prefill binding | Optional prefill mode that writes one diagnostic KV position per processed token into session-owned KV storage. |
| Standalone RoPE op | Position-dependent F32 graph op over a bounded deterministic vector, checked against an independent reference. |

The runtime path after minimal KV ownership proceeds through minimal KV-backed
prefill binding, full transformer prefill, decode, logits, sampling,
generation, CLI generation, and provider generation. Each layer enters the
contract when code, tests, report shape, command proof, and cleanup behavior
exist.

## Graph Execution Contract

Graph execution is admitted through preflight.

A graph path must know which artifact it reads from, which tensor ranges are
valid, which backend owns the selected weights, which operation is supported,
which output memory is planned, and which cleanup path runs after failure.

The controlled fixture graph proves exact executor mechanics over tiny F32
fixtures. It covers graph planning, embed-node ordering, backend dispatch,
output allocation, readback, checksum/comparison behavior, and cleanup.

The selected embedding graph executes a constrained scheduled segment over
engine-attached `token_embd.weight`. It selects a token, converts the selected
F16 embedding row into an F32 output summary, and compares backend output
against an independent raw-artifact reference slice.

The selected embedding-plus-RMSNorm segment extends the real graph path with a
second tensor and a second operation. It reads `token_embd.weight`, applies
RMSNorm using a real first RMSNorm weight such as `blk.0.attn_norm.weight`,
dispatches backend RMSNorm, and reports an F32 output summary checked against an
independent reference.

The standalone RoPE operation is admitted through `yvex graph --execute-op --op
rope`. It does not use a model artifact. It validates backend availability,
backend RoPE support, F32 input/output byte accounting, even positive head
dimension, position, output allocation, dispatch, reference comparison, and
cleanup status. The result reports checksums and max absolute diff against an
independent reference. RoPE support is only a position operation boundary; it is
not attention, QKV projection, transformer block execution, layer scheduling,
decode, logits, sampling, generation, or a provider path.

Every implemented graph result should report guard status, failure phase where
applicable, backend/op status, output planning, reference planning, checksums,
max diff, cleanup status, and the readiness fields that belong to that
boundary.

## Token and Prefill Contract

Token input is a first-class runtime boundary.

`yvex input tokens` parses bounded comma-separated token IDs, validates token
count and integer bounds, and checks token IDs against selected embedding
vocabulary facts when available. Implemented graph paths can consume a selected
token from that validated sequence with `--tokens IDS --token-index N`.

Prompt text becomes token input only through an executable tokenizer path for
the selected artifact. When tokenizer metadata is missing, the command reports
the tokenizer boundary instead of treating prompt text as executable input.

Prefill state is the first state boundary above explicit token input. The
current prefill state summary runs the implemented selected
embedding-plus-RMSNorm segment over each token in order. It records token count,
processed positions, segment execution count, output byte accounting, aggregate
checksum, final-token checksum, max diff, and cleanup status.

`yvex prefill --attach-kv --kv-layers N --kv-heads N --kv-head-dim N
--kv-capacity N` adds a minimal KV-backed binding layer. The command validates
the token sequence, runs the existing segment per token, allocates a
session-owned F32 KV store, writes one deterministic diagnostic KV position per
processed token, reads back position zero, reports KV shape/byte/readback
facts, and cleans up on failure. Capacity smaller than token count fails before
ambiguous state.

The minimal KV binding values are not real attention keys and values. They are
diagnostic state derived from the implemented segment result. Current prefill
summary readiness fields must report the state of the next layers accurately:
minimal KV binding may exist while full transformer prefill, decode, logits,
sampling, and generation remain not ready.

## Minimal KV Contract

Minimal KV is a session-owned storage boundary.

`yvex kv --layers N --heads N --head-dim N --capacity N` creates a bounded F32
KV store and reports shape, byte accounting, allocation, append/read counters,
overflow status, cleanup status, and readiness fields. `--append-demo` appends
deterministic complete positions. `--read-position N` reads one written
position and reports a checksum and sample values.

The shape contract is:

```text
values_per_position = layers * heads * head_dim * 2
bytes_per_position = values_per_position * sizeof(F32)
planned_bytes = bytes_per_position * capacity
```

All arithmetic is checked. Zero shape fields, value-count overflow, byte-count
overflow, append past capacity, reads outside capacity, reads from unwritten
positions, and mismatched position sizes must fail cleanly.

Session-owned KV stores can be created through session options and manipulated
through session KV append/read/clear functions. Session reports mirror KV owner,
dtype, shape, byte counts, append/read counters, overflow state, and cleanup
state.

Minimal KV is not attention execution. The prefill command can bind processed
positions to KV rows with deterministic diagnostic values, but that does not
mean transformer layer outputs are being projected into attention K/V rows.
Minimal KV binding does not make full transformer prefill, decode, logits,
sampling, generation, or provider generation ready.

## Artifact Integrity Contract

Artifact integrity is local correctness and corruption safety for runtime entry.

Before runtime paths read tensor payloads, YVEX validates the artifact structure
needed by that path. The integrity layer checks file size, GGUF parseability,
tensor directory consistency, tensor name uniqueness, supported rank/dims/dtype
accounting, checked byte-count math, tensor offset/range bounds, and required
selected readiness facts for the implemented graph paths.

Tensor byte ranges are validated through a canonical calculation before payload
reads. The calculation checks element count, dtype size, tensor byte count,
`tensor_data_offset + tensor_relative_offset`, end offset, file bounds, and
alignment where applicable. Materialization uses that validated range before
allocation and copy. Real graph paths validate selected token-slice ranges
before raw reference extraction.

Shape and dtype accounting runs before byte-range validation and runtime payload
reads. YVEX rejects unsupported rank, zero dimensions, element-count overflow,
dtype-size ambiguity, tensor byte-count overflow, and selected embedding
shape/dtype mismatch for the implemented paths. Storage recognition and compute
support remain separate facts.

The integrity fixture suite uses tiny GGUF corruption fixtures. It exercises
parser, integrity, materialization, and graph-entry refusal paths before tensor
payloads are trusted. It is regression coverage for implemented boundaries.

## Materialization Gate Contract

Materialization is gated by artifact integrity and backend readiness.

Before backend allocation, YVEX checks structural integrity, local identity when
available, registry metadata drift when aliases are used, shape/dtype
accounting, tensor byte ranges, selected tensor readiness, and backend
availability. Preflight failure stops before backend allocation.

When allocation or transfer fails after preflight, materialization attempts
cleanup and reports the failure phase. The report includes allocation, transfer,
cleanup attempt flags, cleanup result, and planned or transferred byte counts
where available.

Materialization reports local backend lifecycle behavior. They describe
residency, byte accounting, and cleanup for selected tensors.

## Graph Admission Contract

Graph execution is gated by graph-specific admission.

Fixture, selected embedding, and selected segment paths run preflight before
dispatch. The guard composes structural integrity, identity when aliases are
used, metadata drift, shape/dtype accounting, tensor range validation, token
input selection where provided, selected token-slice validation, backend
availability, backend operation support, output allocation sizing, and reference
read bounds.

Preflight failure stops before dispatch. Post-preflight failures attempt cleanup
and report the graph phase. The guard applies to the graph paths currently
implemented. Broader graph safety grows with the graph executor, op set,
transformer block path, layer scheduler, KV-backed prefill, decode, and logits
work.

## Operator Integrity Report

The operator integrity report composes existing local checks into one summary.
It can include artifact structural integrity, local digest identity, registry
metadata drift, shape/dtype accounting, tensor range validation, selected
embedding readiness, materialization preflight when a backend is supplied, and
graph-entry guard status for implemented paths.

A passing report is local operator evidence for the implemented checks. It
should be read as an admission summary for the current artifact and selected
runtime path.

## Server Contract

`./yvexd` exposes the provider/status shell.

Current daemon surfaces:

```text
GET /health
GET /metrics
GET /v1/models
```

The daemon can report health, metrics, model listing, explicit model path or
alias resolution, and generation availability. Runtime-backed provider
generation enters the contract after the runtime owns the generation path
underneath it.

Provider compatibility follows runtime capability. OpenAI-compatible
generation, Anthropic-compatible generation, streaming, tool-call handling, and
provider session behavior need runtime generation, tests, API behavior, and
failure reporting before they become public contract.

## Console Contract

The CLI and console surfaces must reflect runtime state.

`prompt` is a diagnostic renderer. `run` is an accepted-input diagnostic surface
until the runtime generation path exists. `chat` is a diagnostic console and
future canonical REPL. Plain text output must come from real runtime behavior;
fake assistant text should never appear as a substitute for generation.

Future line editing, history, slash commands, and terminal polish belong to
CLI/console code. They should not enter backend core, public runtime ownership,
or headless server paths.

## Output Contract

Standard output is for command payloads and machine-readable output when
requested. Standard error is for errors, diagnostics, and progress. Logs and
runtime artifacts go under generated or runtime output locations only when
requested.

JSON and JSONL output must remain parseable. Logs, progress lines, warnings,
and diagnostics must not be mixed into stdout when stdout is carrying
machine-readable data.

Human-readable output should distinguish pass, warn, fail, and unsupported
states. Colorized output must degrade cleanly for non-TTY output and respect
`NO_COLOR`. Machine-readable output must remain uncolored.

## Validation Contract

Baseline validation:

```sh
make check
make smoke
git diff --check
```

CUDA-capable hosts should also run:

```sh
make check-cuda
```

Documentation and surface checks:

```sh
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

Artifact guardrail:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected artifact state: no tracked large model artifacts. Tracked GGUF files
are tiny fixtures only.

Runtime changes should include command-visible proof, tests, failure-path
coverage, cleanup/lifecycle behavior, and explicit boundary documentation.
Public docs should reflect the implemented boundary without exposing internal
delivery IDs outside `docs/spine.md`.

## Claim Promotion Contract

A runtime claim can be promoted only when implementation, tests, command proof,
failure behavior, cleanup behavior, and documentation exist for the matching
boundary.

Selected materialization promotes residency for selected tensors. Engine
attachment promotes engine-owned selected weight state. Fixture graph execution
promotes deterministic graph execution over controlled fixtures. Selected
embedding execution promotes real selected tensor participation in scheduled
graph work. Embedding-plus-RMSNorm promotes the first multi-op selected real
graph segment. Token input promotes explicit token sequence handling. Prefill
state summary promotes the first sequence-state foundation.

KV, decode, logits, sampling, generation, provider generation,
OpenAI-compatible generation, Anthropic-compatible generation, benchmark
performance, and capability evaluation each need their own implementation and
proof path.

Benchmark numbers require the measured runtime path, model/artifact identity,
backend, qtype, context length, machine, command, and reproducibility notes.

The contract should keep the runtime honest: every claim points to the exact
state the code can create today.
