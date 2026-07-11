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

YVEX is a CLI-first local runtime intended to execute complete open-weight
model artifacts. The exact v0.1.0 target is currently unsupported.

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

Complete model artifacts stay outside the repository. Local registries may contain
absolute paths because they are machine-local operator state. They must remain
untracked.

Tracked GGUF files are reserved for tiny fixtures under `tests/`. Real model
weights, generated complete model artifacts, local reports, local logs, cache files, and
machine-specific registries stay outside git.

The repository may contain compact fixtures, structural corruption fixtures,
small vectors, and regression data. A one-tensor or bounded-subset file is a
tensor proof artifact, not a complete or supported model artifact.

### Native GGUF Reader Contract

The canonical GGUF v3 structural reader uses bounded positioned file reads. It
owns its decoded container, metadata, names, tensor directory, indexes, qtype
projection, and addressable range facts; callers may close the artifact handle
after a successful open. Borrowed accessor results remain valid only until the
parsed GGUF view is closed.

Structural parsing stops at the aligned tensor-data boundary. Reader statistics
must report zero tensor payload bytes read, and parser memory is bounded by
explicit metadata, tensor, array, string, and owned-byte budgets rather than by
the artifact file size. A report may render a typed rejection, but successful
report construction is not parser acceptance.

The reader does not itself admit global tensor ordering or padding. The
canonical layout validator consumes the reader view and opened artifact,
requires power-of-two alignment and exact directory-order padded continuation,
checks all required padding for zero bytes, validates the exact aggregate file
span, and detects snapshot drift. It reads no tensor payload bytes. This layout
result is still not complete-artifact, materialization, or runtime support.

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
artifact paths. A registry entry may record file identity, format,
architecture, tensor count, known tensor bytes, primary tensor facts, and
legacy selected-proof fields. Those fields do not make the referenced file a
complete or supported model artifact.

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

CPU is the reference backend for bounded proofs. CUDA has implemented bounded
materialization and primitive paths, but it is not a supported model backend.
`V010.CUDA.FAILCLOSED.0` remains priority-blocking because the non-nvcc fallback
can expose no-op kernel entry points.

CUDA implementation lives under `cuda/`. CUDA host bridge code stays in C
translation units. CUDA device kernels live in `.cu` translation units. CUDA
availability is optional for baseline validation and required for CUDA
validation.

Backend failures must remain distinguishable. A caller or operator should be
able to tell whether failure came from backend discovery, allocation, transfer,
operation support, dispatch, output readback, or cleanup.

Materialization is the residency boundary. Current bounded paths move tensor
proof bytes into backend-owned storage and prove only allocation, transfer,
byte accounting, and cleanup for that subset. Full model residency requires
every tensor in the complete model artifact.

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

The implemented proof and ownership surfaces are:

| Surface | Current contract |
| --- | --- |
| Artifact facts | Parsed from local artifact paths and reported through GGUF/model/tensor surfaces. |
| Descriptor proof | Built from parsed artifact facts and bounded tensor roles; not execution-complete. |
| Backend transfer proof | Created by bounded tensor materialization on CPU or CUDA; not full residency. |
| Engine attachment proof | Bounded backend-resident weights attached to engine-owned state. |
| Session visibility | Sessions can observe engine attachment summaries. |
| Controlled graph result | Deterministic fixture graph execution over tiny F32 fixtures. |
| Tensor graph proof | Bounded embedding execution over F16 `token_embd.weight`. |
| Segment graph proof | Embedding-plus-RMSNorm execution over bounded source tensors. |
| Token input | Bounded explicit token sequences with validation and token-index selection. |
| Prefill proof summary | Segment-summary diagnostic state over validated explicit token input; not transformer prefill. |
| KV storage proof | Session-owned F32 storage with shape, append/read, clear, overflow, and cleanup reports; not attention-backed KV. |
| Diagnostic KV binding | Writes one deterministic proof position per processed token; not model K/V. |
| Standalone RoPE op | Position-dependent F32 graph op over a bounded deterministic vector, checked against an independent reference. |
| Standalone attention op | Explicit F32 Q/K/V scaled dot-product attention primitive with bounded causal mask, scratch, output, cleanup, and reference comparison. |
| Standalone matmul op | Explicit F32 row-major `input=[m,k]`, `weight=[k,n]`, `output=[m,n]` primitive with projection-shape reporting, output cleanup, and reference comparison. |
| Standalone MLP op | Explicit F32 gated SiLU feed-forward primitive over deterministic dense weights or one bounded routed expert slice, with intermediate/output cleanup and reference comparison. |

These are implementation facts, not a runtime progress ladder. None closes
complete artifact, materialization, executable descriptor, transformer,
generation, evaluation, benchmark, or release gates. The product runtime path
is defined only by `PROJECT.md`.

## Graph Execution Contract

Graph execution is admitted through preflight. The bounded and primitive paths
described below are retained technical contracts pending the decommission map;
they are not product runtime capability.

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

The standalone attention operation is admitted through `yvex graph
--execute-op --op attention`. It does not use a model artifact and does not
project Q/K/V from model tensors. It validates backend availability, backend
attention support, explicit F32 query/key/value shapes, `seq_len`, `position`,
positive `head_dim`, causal mask bounds, score/probability scratch sizing,
output allocation, dispatch, reference comparison, and cleanup status. The
result reports checksums, softmax probability diff, output max absolute diff,
visible and masked key counts, dispatch/reference/allocation/cleanup fields, and
unsupported readiness fields. Attention primitive support is not transformer
block execution, layer scheduling, full transformer prefill, decode, logits,
sampling, generation, or a provider path.

The standalone matmul operation is admitted through `yvex graph --execute-op
--op matmul`. It does not use a model artifact and does not read model
projection weights. It validates backend availability, backend matmul support,
explicit F32 rank-2 input/weight/output shapes, non-zero `m`, `k`, and `n`,
byte-count accounting, output allocation, dispatch, reference comparison, and
cleanup status. The result reports projection shape (`m=1`) separately from
non-projection matrix shape, byte counts, checksums, max absolute diff, and
unsupported readiness fields. Matmul primitive support is not Q/K/V projection
readiness, attention integration, transformer block execution, layer
scheduling, full transformer prefill, decode, logits, sampling, generation, or
a provider path.

The standalone MLP operation is admitted through `yvex graph --execute-op --op
mlp`. It does not use a model artifact and does not read real MLP or expert
weights. It validates backend availability, backend MLP support, explicit F32
input/gate/up/down/intermediate/output shapes, non-zero hidden and feed-forward
dimensions, gated `silu` activation, optional explicit expert count and
`expert_id`, byte-count accounting, output allocation, dispatch, reference
comparison, and cleanup status. Routed-expert mode selects one deterministic
expert slice only. MLP primitive support is not router-logit computation, top-k
routing, real MoE routing, transformer block execution, layer scheduling, full
transformer prefill, decode, logits, sampling, generation, or a provider path.

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
the bounded proof input. When tokenizer metadata is missing, the command reports
the tokenizer boundary instead of treating prompt text as executable input.

The current prefill proof summary is a diagnostic state boundary above explicit
token input. It runs the bounded
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

The minimal KV binding values are not attention keys and values. They are
deterministic diagnostic state derived from the bounded segment result. Their
existence does not promote prefill, KV, decode, logits, sampling, or generation.

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
sampling, generation, or provider generation-ready state.

## Artifact Integrity Contract

Artifact integrity is local correctness and corruption safety for runtime entry.

Before runtime paths read tensor payloads, YVEX validates the artifact structure
needed by that path. The integrity layer consumes the canonical global layout
result, including ordered qtype-sized spans, exact padded continuation, zero
padding, truncation/tail policy, and snapshot stability. It adds optional
explicit digest policy and subordinate selected readiness facts for implemented
graph paths. It does not hash the complete file unless identity policy requests
a digest.

Tensor byte ranges are validated through the canonical GGUF qtype storage
calculation before payload reads. The calculation treats `dims[0]` as the row
width, requires exact block divisibility, derives row count from the remaining
dimensions, and checks row, element, and total-byte arithmetic before computing
`tensor_data_offset + tensor_relative_offset`, end offset, file bounds, and
alignment where applicable. Materialization uses that validated range before
allocation and copy. Real graph paths validate selected token-slice ranges
before raw reference extraction.

Shape and dtype accounting runs before byte-range validation and runtime payload
reads. YVEX rejects unsupported rank, zero dimensions, row-width/block
mismatch, element-count overflow, row-byte overflow, row-count overflow,
total-byte overflow, and selected embedding shape/dtype mismatch for the
implemented paths. Storage admission, reference decoding, quantization,
emission, and backend compute support remain separate facts.

The integrity fixture suite uses tiny GGUF corruption fixtures. It exercises
parser, integrity, materialization, and graph-entry refusal paths before tensor
payloads are trusted. It is regression coverage for implemented boundaries.

## Materialization Gate Contract

Materialization is gated by artifact integrity and backend readiness.

Before backend allocation, YVEX checks canonical global layout, local identity when
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

Expected artifact state: no tracked model payloads or complete model artifacts.
Tracked GGUF files are tiny fixtures only.

Runtime changes should include command-visible proof, tests, failure-path
coverage, cleanup/lifecycle behavior, and explicit boundary documentation.
Public docs should reflect the implemented boundary without exposing internal
delivery IDs outside `PROJECT.md`.

## Claim Promotion Contract

A runtime claim can be promoted only when implementation, tests, command proof,
failure behavior, cleanup behavior, and documentation exist for the matching
boundary.

Tensor proof materialization, fixture execution, primitive comparison, bounded
segments, token parsing, diagnostic KV storage, reports, and command grammar
promote only their exact lower-level contracts. They do not promote model
residency, transformer execution, runtime generation, provider behavior,
evaluation, benchmark, or release state.

Every product gate in `PROJECT.md` and `docs/v010-release-doctrine.md` needs
its own implementation, reference or acceptance result, refusal path, cleanup,
operator proof, and focused guard.

Benchmark numbers require the measured runtime path, model/artifact identity,
backend, qtype, context length, machine, command, and reproducibility notes.

The contract should keep the runtime honest: every claim points to the exact
state the code can create today.
