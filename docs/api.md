# YVEX API

This file describes the public C API surface for YVEX.

The API exists to expose the same boundaries that the runtime enforces
internally: local artifacts, tensor facts, backend ownership, engine/session
lifetime, graph execution, token input, prefill state summaries, integrity
reports, and operator-facing diagnostics. It is a map of the public headers and
the contracts that are already meaningful to external C consumers.

YVEX is still early in the full local transformer path, so the API is
intentionally conservative. Public headers should describe implemented behavior,
stable report shapes, explicit ownership rules, and clear unavailable
boundaries. Future runtime stages such as full KV, decode, logits, sampling, and
generation should enter the public API only when the runtime has code, tests,
command-visible proof, and documented failure behavior.

The goal is simple: a C consumer should be able to call into YVEX and
understand who owns the memory, which state has been created, which boundary
failed, and which runtime claim is actually backed by implementation.

## Public include surface

Use the umbrella header when consuming the implemented public surface:

```c
#include <yvex/yvex.h>
```

The umbrella header collects the public API groups below.

| Area | Headers | Public role |
| --- | --- | --- |
| Core runtime facts | `version.h`, `status.h`, `error.h`, `log.h` | Versioning, status values, error reporting, and logging contracts. |
| Artifact and GGUF | `artifact.h`, `artifact_identity.h`, `artifact_integrity.h`, `artifact_naming.h`, `gguf.h`, `gguf_emit.h`, `gguf_template.h` | Local artifact opening, naming, identity, integrity, GGUF parsing, controlled emission, and template validation. |
| Model and tensors | `dtype.h`, `tensor.h`, `model.h`, `weights.h` | Dtype metadata, tensor descriptors, model descriptors, selected weights, and backend-resident weight tables. |
| Tokenizer and prompt | `tokenizer.h`, `token_input.h`, `prompt.h` | Tokenizer fixture paths, explicit token input objects, and prompt/rendering diagnostics. |
| Graph and planning | `graph.h`, `op.h`, `planner.h`, `memory_plan.h` | Graph result reports, op boundaries, planning structures, and memory-plan summaries. |
| Backend | `backend.h` | Backend discovery, capability reports, tensor allocation, transfer, and operation surfaces. |
| Runtime and session | `engine.h`, `session.h`, `kv.h`, `logits.h` | Engine ownership, session visibility, current KV/logits boundary headers, and future runtime-state integration points. |
| Metrics and traces | `metrics.h`, `trace.h`, `profile.h` | Runtime summaries, traces, profiles, and future measurement surfaces. |
| Server | `server.h` | Provider/status shell structures and server-facing runtime reports. |
| Tool APIs | `source_manifest.h`, `native_weights.h`, `conversion.h`, `weight_mapping.h`, `qtype_support.h`, `quant_policy.h`, `quant_job.h`, `imatrix.h`, `model_registry.h`, `model_ref.h`, `model_gate.h`, `materialize_gate.h` | Source provenance, native tensor inventory, conversion planning, tensor mapping, qtype policy, quantization evidence, registry, model resolution, and gate reports. |
| Filesystem | `fs.h` | Runtime filesystem helpers and local path handling. |

The header list is intentionally broad because the engine exposes several
layers that many tools hide behind one word such as "loaded". The API keeps
those layers separate so callers can build tools without guessing which state
exists.

## Error model

Public functions return integer status codes. `YVEX_OK` means success. Failure
returns an explicit status value, and APIs that accept a `yvex_error` write a
bounded explanation into the caller-provided error object.

The error model is designed around operational diagnosis. Invalid arguments,
missing files, parse failures, unsupported storage or compute paths, backend
availability failures, allocation failures, copy failures, graph guard failures,
and integrity failures should remain distinguishable. A caller should be able
to tell whether the problem happened before file parsing, during artifact
validation, during backend materialization, during graph preflight, during
dispatch, or during cleanup.

Errors should be specific without transferring ownership. Error objects and
report structs contain copied scalar fields, copied bounded strings, or string
literals with stable meaning. They should never require callers to keep a
parser, backend tensor, or engine object alive only to read a report.

## Ownership model

YVEX uses explicit ownership rules.

Public option structs borrow input pointers for the duration of the call.
Returned opaque handles are owned by the caller and must be released by the
matching close or free function. Registry and resolver objects own copied
strings until they are cleared or closed. Backend tensors are released through
backend or weight-table close paths. Engine-attached weights are released by
engine close. Sessions can observe engine attachment state while the engine owns
the attached weights.

Model artifacts remain operator-local. The API can open, identify, register,
validate, and report on them, but it should not turn the repository into
artifact storage and should not hide local file identity behind global claims.

The ownership model follows the runtime path:

```text
artifact path
  -> parsed artifact and tensor facts
  -> descriptor
  -> selected backend tensor
  -> engine-attached weight table
  -> session-visible runtime state
  -> graph result or prefill summary
```

Each step has a different lifetime. The API should keep those lifetimes visible.

## Current capability boundary

The public API currently covers the lower runtime path: artifact inspection,
descriptor construction, selected materialization, engine/session attachment,
controlled graph execution, selected embedding execution,
embedding-plus-RMSNorm execution, explicit token input, prefill state summary,
and integrity reporting.

| Area | Current boundary |
| --- | --- |
| GGUF metadata and tensor parsing | Implemented for local GGUF artifacts and tiny fixtures. |
| Tensor and model descriptors | Implemented for parsed model facts and selected tensor roles. |
| Tokenizer fixture path | Implemented for fixture and diagnostics paths. |
| Explicit token input | Implemented for bounded token sequences and token-index selection. |
| Prompt rendering diagnostics | Implemented as diagnostics, with prompt text promoted to token input only through available tokenizer metadata. |
| Graph and planner substrate | Implemented for current controlled and selected graph paths. |
| CPU backend | Implemented for reference materialization and graph execution paths. |
| CUDA backend | Implemented for probe, tensor movement, parity subsets, and selected graph paths where CUDA is available. |
| Selected tensor materialization | Implemented for selected artifacts. |
| Engine-owned weight attachment | Implemented for selected materialized weights. |
| Session visibility | Implemented for observing engine-attached weight state. |
| Controlled fixture graph | Implemented over deterministic F32 fixtures. |
| Selected embedding graph | Implemented for F16 `token_embd.weight`. |
| Selected embedding-plus-RMSNorm graph | Implemented for selected F16 embedding plus first RMSNorm weight. |
| Prefill state summary | Implemented as a segment-summary foundation over explicit token sequences. |
| Artifact identity | Implemented with file size and SHA-256 digest. |
| Artifact integrity | Implemented for structural GGUF checks, range checks, shape/dtype accounting, selected readiness, and local digest expectations. |
| Registry metadata drift | Implemented for local alias summaries. |
| Local model registry | Implemented. |
| Alias-or-path model resolver | Implemented for one-shot commands and daemon status paths. |
| Model and materialization gates | Implemented as local readiness and preflight reports. |
| Server/provider status shell | Implemented for health, metrics, model listing, and generation availability reporting. |
| KV runtime | Public boundary header exists; full runtime ownership arrives with the KV implementation path. |
| Logits runtime | Public boundary header exists; logits buffers arrive with the logits production path. |
| Generation and benchmarks | Public C API waits for the runtime generation and benchmark paths to exist. |

The table is a capability map, not a marketing surface. Each row describes what
a caller can safely treat as an API boundary today.

## Artifact identity

`yvex_artifact_identity_read` and `yvex_artifact_compute_sha256` provide local
file identity evidence. The identity surface reports current file size and
lowercase SHA-256 digest. The hashing path streams the file, so large artifacts
do not need to be loaded into memory just to compute identity.

A digest match means the local bytes match a recorded or expected local value.
It is useful for registry drift detection, operator repeatability, and gate
checks. It should be treated as local identity evidence, not as author identity,
remote provenance, model quality, or external trust.

Registry entries can carry a bounded metadata summary for alias drift
diagnostics: support level, format, architecture, tensor count, known tensor
bytes, primary tensor name, role, dtype, rank, dimensions, byte count, and
selected embedding readiness facts. `yvex_model_registry_compare_metadata`
compares a registered summary with current artifact facts and fills a
caller-owned `yvex_model_metadata_drift_report` with status fields and a
bounded issue list.

The registry comparison borrows artifact facts and writes copied report data.
It does not own artifact objects and does not turn a local alias into a global
model identity.

## Artifact integrity

`yvex_artifact_integrity_check_path` opens a local artifact path and returns a
caller-owned `yvex_artifact_integrity_report`.

`yvex_artifact_integrity_validate` borrows an already opened artifact, parsed
GGUF object, and tensor table. It writes a report without taking ownership of
those objects.

The integrity report includes structural status, local identity and digest
status for path checks, selected embedding readiness where requested, and a
bounded copied issue list. The current integrity surface checks GGUF structure,
tensor directory consistency, shape and dtype accounting, byte ranges, selected
token slices, and optional expected or registered SHA-256 matching.

The tiny corrupt GGUF fixtures in the test suite exercise parser-to-integrity
error mapping and bounded issue reporting. That corpus is structural regression
coverage for the implemented parser and validator paths.

Integrity is an admission layer before materialization and graph execution. It
helps the runtime stop early when local artifact facts are inconsistent with the
work a later stage would perform.

## Tensor accounting and range validation

`yvex_tensor_shape_accounting_validate` is the canonical shape and dtype
accounting step. It reports rank, dimensions, element count, storage byte
accounting, storage support, and the narrow compute-support flags used by the
current controlled fixture and selected embedding paths.

`yvex_tensor_range_validate` is the canonical byte-range calculation for a
parsed tensor directory row. It computes element count, dtype size, tensor byte
count, `tensor_data_offset + tensor_relative_offset`, absolute start, absolute
end, file bounds, and alignment status before payload-reading paths access
tensor bytes.

`yvex_selected_embedding_shape_validate` interprets `token_embd.weight` as
`dims[0] = hidden_size` and `dims[1] = vocab_size`, validates a token id, and
reports output count, output bytes, and selected-token slice bytes for the real
partial graph path.

`yvex_tensor_embedding_slice_range_validate` narrows the validated tensor range
to the selected embedding token slice used by the current real partial graph
path.

These helpers prove that the runtime knows which bytes it may read before
materialization, reference extraction, or graph execution. They are the
foundation for larger tensor and graph paths.

## Materialization

Materialization moves selected tensor bytes from parsed artifact storage into
backend-owned runtime storage.

The public materialization reports include gate status, failure phase, integrity
status, shape and range status, backend status, allocation attempt, transfer
attempt, cleanup attempt, cleanup result, and planned or transferred byte counts
where available.

These report fields are copied scalar or string facts. They do not transfer
backend tensor ownership to the caller. Backend tensor lifetime remains
controlled by backend tensor objects, weight tables, and engine close paths.

Materialization is the residency boundary. It proves that selected bytes can
become backend-owned memory with visible accounting and cleanup behavior.

## Engine and session

The engine owns attached selected materialized weights. Engine close releases
the attached state.

Sessions observe engine attachment state. A session can report that weights are
attached and visible, while ownership remains with the engine. This keeps
lifetime simple: the engine owns the weight table, and the session sees the
engine state it is attached to.

The current engine/session API is intentionally focused on selected weight
attachment, graph execution, token input, and prefill state summaries. As KV,
decode, logits, and generation become implemented runtime states, their
ownership must follow the same rule: the object that owns the memory also owns
the cleanup path, and reports must make that visible.

## Graph execution

`yvex_engine_execute_partial_graph` borrows engine-attached weights owned by
`yvex_engine` and returns a copied output summary. The executor hides backend
pointers from the caller and keeps ownership inside the engine/backend path.

The implemented partial graph boundary is the selected token-embedding segment
over `token_embd.weight`. The result reports graph guard status, selected
tensor facts, backend status, output shape, output checksum, reference checksum,
max diff, sample values, and readiness fields for the current execution
boundary.

`yvex_engine_execute_segment_graph` borrows the same engine-attached weight
state and executes the implemented selected segment kind, `embedding-rmsnorm`.
The result reports the selected embedding tensor, the RMSNorm tensor, epsilon
metadata, memory planning for intermediate, output, scratch, and reference
buffers, output and reference checksums, sample values, and max absolute
difference against an independent raw-artifact reference.

The segment accepts a selected token id. Prompt and token input normalization
happen before this call. The current segment is embedding lookup followed by
RMSNorm, and it forms the first multi-op real-model graph slice in the public
API.

`yvex_backend_op_rms_norm` is the backend RMSNorm op used by the selected
segment. It supports the narrow path needed by the current graph boundary: F32
input/output, F16 or F32 RMSNorm weights, and explicit epsilon.

The graph API is expected to grow through explicit op, block, layer, prefill,
decode, logits, and generation boundaries. Each new layer should add report
fields and ownership rules at the point where the runtime can prove behavior.

## Token input

`yvex_token_input_parse_explicit`, `yvex_token_input_validate_bounds`, and
`yvex_token_input_select` define the public token input boundary.

The token input object owns a bounded copied token list. It can be validated
against vocabulary facts and selected by index before graph execution. Prompt
text becomes token input through tokenizer APIs when the artifact has executable
tokenizer metadata and the runtime path supports that conversion.

Token input is the first API layer where user-supplied token sequences become
structured runtime input. It sits above artifact/tensor readiness and below
prefill state.

## Prefill state summary

`yvex_engine_create_prefill_state` consumes a validated `yvex_token_input` and
returns a copied `yvex_prefill_state_summary`.

The current summary is a segment-summary foundation built by running the
implemented `embedding-rmsnorm` segment independently for each token in order.
It reports token count, processed positions, segment execution count, output
bytes, scratch bytes, aggregate checksum, final-token checksum, max diff,
cleanup status, and readiness fields for the next runtime layers.

This is the first public API surface where a token sequence becomes runtime
state. It gives the next KV implementation a concrete predecessor: positions,
processed token count, per-token graph work, aggregation, cleanup behavior, and
state reporting.

KV-backed transformer prefill will extend this path by attaching token ranges
to session-owned KV state. Decode, logits, sampling, and generation should then
build on top of that same state chain.

## Operator integrity report

The operator integrity report is a CLI aggregation of existing public report
facts. It introduces no new ownership surface.

It summarizes artifact integrity, local digest identity, registry metadata
drift, shape and dtype accounting, tensor range validation, selected embedding
readiness, materialization preflight, and graph-entry guard status for
implemented paths.

A passing report is local operator evidence. It says the implemented checks
passed for the local artifact and selected runtime path. It does not replace
external model provenance, model evaluation, or future generation validation.

Together, artifact identity reports, artifact integrity reports, tensor range
reports, shape accounting, registry metadata drift reports, materialization
gate summaries, graph guard summaries, and operator integrity reports form the
closed artifact-integrity surface. They are caller-owned reports with copied
scalar or string fields. They do not transfer artifact ownership, backend
ownership, or execution ownership.

## Server status surface

`server.h` describes the public server/status boundary.

The current daemon surface is useful for process health, metrics, model listing,
direct path or alias resolution, and generation availability reporting. It
gives provider-shaped code a way to observe runtime state before
provider-backed generation exists.

Server generation, OpenAI-compatible generation, Anthropic-compatible
generation, streaming responses, tool-call handling, and provider-level session
behavior should enter public API documentation when the runtime generation path
exists underneath them.

## Metrics, traces, and profiles

The metrics, trace, and profile headers describe the reporting layer for
runtime behavior.

The current project uses command-visible reports, summaries, checksums, max-diff
fields, cleanup fields, and failure phases as the primary observability path. As
the engine grows into KV, decode, logits, sampling, and generation, these
headers should carry the runtime evidence needed to debug long executions:
artifact identity, backend, memory pressure, graph phase, token range, KV state,
logits state, sampling parameters, and server request context.

The reporting model should remain tied to real runtime boundaries. A metric is
useful when it says which state was measured and which command or API path
produced it.

## API extension rules

Public API growth follows runtime proof.

A new public type, function, or report field should land with implementation,
tests, failure behavior, ownership rules, and documentation. Future-looking
headers may mark boundary names, but stable contracts should describe behavior
the runtime can actually exercise.

The extension path is:

```text
artifact facts
  -> descriptor
  -> materialization
  -> engine/session attachment
  -> graph execution
  -> token input
  -> prefill state
  -> KV ownership
  -> decode
  -> logits
  -> sampling
  -> generation
  -> server/provider generation
  -> evaluation and benchmarks
```

The API should keep these stages separate. Materialization should not imply
graph execution. Graph execution should not imply logits. Logits should not
imply sampling. Sampling should not imply a provider server. Each stage should
have its own ownership and report shape.

## Validation expectations

API changes should be validated through the same style as runtime changes:

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
make check-cuda
```

When an API change affects artifact identity, integrity, materialization, graph
execution, token input, or prefill state, the corresponding CLI and regression
tests should prove the report fields, ownership behavior, cleanup behavior, and
failure phases.

Public headers are part of the contract. A header change should be treated as a
runtime surface change, not as an internal refactor.
