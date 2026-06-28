# YVEX API

This document describes the public C API surface of YVEX.

The API mirrors the runtime structure of the project: local artifacts, tensor
facts, backend-owned memory, engine/session lifetime, graph execution, token
input, prefill state summaries, integrity reports, and server-facing
diagnostics. It is written for C consumers that need to understand what they can
include, which objects they own, which reports are copied, and which runtime
boundary a function belongs to.

YVEX exposes a staged local inference engine. A caller should be able to move
from artifact evidence to descriptors, from descriptors to selected backend
residency, from residency to engine-owned state, from engine-owned state to
graph execution, and from token input to prefill summaries. Later layers such as
KV ownership, decode, logits, sampling, generation, and provider serving extend
the same chain as their runtime contracts mature.

This file is the API map. Runtime behavior is governed by
[docs/contract.md](contract.md). Operator command transcripts live in
[docs/operator-runbook.md](operator-runbook.md). The public project overview
lives in [README.md](../README.md).

## Include Surface

Most consumers should include the umbrella header:

```c
#include <yvex/yvex.h>
```

The umbrella header collects the supported public header groups.

| Area | Headers | Purpose |
| --- | --- | --- |
| Core | `version.h`, `status.h`, `error.h`, `log.h` | Version values, status codes, bounded error reports, and logging contracts. |
| Artifacts and GGUF | `artifact.h`, `artifact_identity.h`, `artifact_integrity.h`, `artifact_naming.h`, `gguf.h`, `gguf_emit.h`, `gguf_template.h` | Local artifact handling, GGUF parsing, controlled emission, identity evidence, and integrity reports. |
| Model and tensors | `dtype.h`, `tensor.h`, `model.h`, `weights.h` | Dtype facts, tensor descriptors, model descriptors, selected weights, and backend-resident weight tables. |
| Tokenizer and input | `tokenizer.h`, `token_input.h`, `prompt.h` | Tokenizer diagnostics, explicit token input objects, prompt rendering, and input normalization boundaries. |
| Graph and planning | `graph.h`, `op.h`, `planner.h`, `memory_plan.h` | Graph reports, operation boundaries, planning structures, and memory planning summaries. |
| Backend | `backend.h` | Backend discovery, capability reports, tensor allocation, transfer, and backend operation calls. |
| Runtime and session | `engine.h`, `session.h`, `kv.h`, `logits.h` | Engine ownership, session visibility, KV/logits boundary types, and runtime-state integration points. |
| Metrics and traces | `metrics.h`, `trace.h`, `profile.h` | Runtime summaries, trace records, profiles, and measurement/reporting structures. |
| Server | `server.h` | Daemon/status structures and server-facing runtime reports. |
| Tooling APIs | `source_manifest.h`, `native_weights.h`, `conversion.h`, `weight_mapping.h`, `qtype_support.h`, `quant_policy.h`, `quant_job.h`, `imatrix.h`, `model_registry.h`, `model_ref.h`, `model_gate.h`, `materialize_gate.h` | Source provenance, native tensor inventory, conversion planning, tensor mapping, qtype policy, quantization evidence, local registry, model references, and gate reports. |
| Filesystem | `fs.h` | Runtime filesystem helpers and local path handling. |

The header groups are broad because the engine makes several layers visible
instead of folding them into a single "load model" operation. A consumer can use
only the slice it needs: artifact inspection, registry resolution,
materialization, graph execution, token input, or reporting.

## Status and Error Model

Public functions return integer status codes. `YVEX_OK` represents success.
Other status values identify the boundary that rejected or failed the
operation.

APIs that accept a `yvex_error` write a bounded explanation into the
caller-provided object. Error reports are copied data. They should be readable
after the callee returns and should never depend on a hidden backend pointer or
temporary parser object.

The error model is operational. A caller should be able to distinguish invalid
arguments, missing files, parse failures, artifact-integrity failures, digest
drift, unsupported storage or compute paths, backend discovery failures,
allocation failures, transfer failures, graph admission failures, dispatch
failures, reference-read failures, and cleanup failures.

The purpose of the error model is to keep runtime boundaries visible. A failure
before parsing is different from a failure during materialization, and both are
different from a graph guard refusing dispatch.

## Ownership and Lifetime

YVEX uses explicit ownership.

Public option structs borrow caller-provided pointers for the duration of the
call. Returned opaque handles are owned by the caller and must be released with
the matching close or free function. Registry and resolver objects own copied
strings until they are cleared or closed. Backend tensors are released through
backend or selected-weight close paths. Engine-attached weights are released by
engine close. Sessions observe engine state through defined session reports.

The runtime path has distinct ownership layers:

```text
artifact path
  -> parsed artifact and tensor facts
  -> runtime descriptor
  -> selected backend tensor
  -> engine-attached weight table
  -> session-visible runtime state
  -> graph result or prefill summary
```

Each layer has its own lifetime. The API should preserve that structure. A
descriptor does not own backend memory. A session observing engine-attached
weights does not free them. A copied report does not keep the graph alive. A
materialization summary does not transfer backend tensor ownership to the
caller.

Model artifacts remain operator-local. The API can open, identify, register,
validate, and report on local artifacts, while real weights stay outside the
repository.

## Public Surface Map

The public API is organized by runtime boundary rather than by implementation
milestone.

| Boundary | Public surface |
| --- | --- |
| Artifact facts | Local artifact opening, GGUF metadata, tensor directory parsing, naming, identity, and integrity reports. |
| Tensor interpretation | Dtype facts, tensor descriptors, shape accounting, range validation, selected embedding readiness, and model descriptors. |
| Model references | Local registry entries, alias-or-path resolution, model references, model gates, and materialization gates. |
| Backend residency | Backend discovery, selected tensor allocation, transfer, release, and materialization summaries. |
| Engine ownership | Engine creation, selected weight attachment, engine-owned lifetime, and graph execution entry points. |
| Session visibility | Session reports over engine-attached runtime state and future session-owned state. |
| Graph execution | Controlled fixture graph results, selected embedding graph results, embedding-plus-RMSNorm segment results, graph guards, checksums, and max-diff reports. |
| Token input | Bounded explicit token lists, token validation, token selection, and prompt-to-token boundaries through tokenizer paths. |
| Prefill state | Segment-summary prefill reports over validated token sequences. |
| Runtime reporting | Metrics, traces, profiles, integrity reports, materialization reports, graph reports, and failure phases. |
| Server status | Daemon health, metrics, model listing, model reference resolution, and generation availability reports. |

This map is the public shape of the API. It describes where a consumer can
connect to YVEX and how those surfaces relate to the larger inference runtime.

## Artifact Identity

`yvex_artifact_identity_read` and `yvex_artifact_compute_sha256` provide local
file identity evidence.

The identity surface reports current file size and lowercase SHA-256 digest.
The hashing path streams the file, so large artifacts can be identified without
loading the whole artifact into memory.

A digest match means the current local bytes match a recorded or expected local
value. This is useful for registry drift detection, repeatable local runs, and
gate checks.

Registry entries may carry bounded metadata summaries: support level, format,
architecture, tensor count, known tensor bytes, primary tensor name, role,
dtype, rank, dimensions, byte count, and selected embedding readiness facts.

`yvex_model_registry_compare_metadata` compares a registered summary with
current artifact facts and fills a caller-owned
`yvex_model_metadata_drift_report`. The report contains copied status fields and
a bounded issue list.

The registry comparison borrows artifact facts and writes report data. It does
not own artifact objects.

## Artifact Integrity

`yvex_artifact_integrity_check_path` opens a local artifact path and returns a
caller-owned `yvex_artifact_integrity_report`.

`yvex_artifact_integrity_validate` borrows an already opened artifact, parsed
GGUF object, and tensor table. It writes a report without taking ownership of
those objects.

The integrity report contains structural status, local identity and digest
status for path checks, selected-readiness facts where requested, and a bounded
copied issue list.

The integrity surface covers the local artifact facts required before
materialization and graph execution: GGUF structure, tensor directory
consistency, shape and dtype accounting, byte ranges, selected token slices, and
optional expected or registered SHA-256 matching.

The tiny corrupt GGUF fixtures in the test suite exercise parser-to-integrity
error mapping and bounded issue reporting. They provide structural regression
coverage for the parser and validator paths.

Integrity acts as an admission layer. It lets the runtime stop before a later
stage trusts inconsistent artifact facts.

## Tensor Accounting and Range Validation

`yvex_tensor_shape_accounting_validate` is the canonical shape and dtype
accounting step. It reports rank, dimensions, element count, storage byte
accounting, storage support, and the narrow compute-support flags used by
current graph paths.

`yvex_tensor_range_validate` is the canonical byte-range calculation for a
parsed tensor directory row. It computes element count, dtype size, tensor byte
count, `tensor_data_offset + tensor_relative_offset`, absolute start, absolute
end, file bounds, and alignment status before payload-reading paths access
tensor bytes.

`yvex_selected_embedding_shape_validate` interprets `token_embd.weight` as
`dims[0] = hidden_size` and `dims[1] = vocab_size`. It validates a token id and
reports output count, output bytes, and selected-token slice bytes for the
selected embedding graph path.

`yvex_tensor_embedding_slice_range_validate` narrows the validated tensor range
to the selected token slice used by the selected embedding graph path.

These helpers give graph and materialization code bounded tensor facts before
memory is read, copied, or compared.

## Materialization

Materialization moves selected tensor bytes from artifact storage into
backend-owned runtime storage.

The public materialization reports include gate status, failure phase, integrity
status, shape and range status, backend status, allocation attempt, transfer
attempt, cleanup attempt, cleanup result, and planned or transferred byte counts
where available.

These fields are copied report facts. Backend tensor lifetime remains
controlled by backend tensor objects, selected-weight tables, and engine close
paths.

Materialization is the residency boundary. It tells a caller that selected
bytes reached backend-owned memory with visible accounting and lifecycle
behavior.

## Engine and Session

The engine owns attached selected materialized weights. Engine close releases
attached engine state.

Sessions observe engine state through session reports. A session can report
that selected weights are attached and visible while the engine retains
ownership. This keeps the lifetime model clear: the engine owns the weight
table, and the session observes the runtime state it is attached to.

As KV, decode, logits, sampling, and generation become runtime states, their API
surfaces should follow the same pattern. The object that owns memory owns
cleanup. The report that summarizes state is copied. The session-level
relationship must stay visible.

## Graph Execution

`yvex_engine_execute_partial_graph` borrows engine-attached weights owned by
`yvex_engine` and returns a copied output summary. Backend pointers remain
inside the engine/backend path.

The partial graph surface covers the selected token-embedding path over
`token_embd.weight`. The result reports graph guard status, selected tensor
facts, backend status, output shape, output checksum, reference checksum, max
diff, sample values, and boundary readiness fields.

`yvex_engine_execute_segment_graph` borrows the same engine-attached weight
state and executes the selected segment kind `embedding-rmsnorm`. The result
reports the selected embedding tensor, the RMSNorm tensor, epsilon metadata,
memory planning for intermediate, output, scratch, and reference buffers, output
and reference checksums, sample values, and max absolute difference against an
independent raw-artifact reference.

The segment accepts a selected token id. Prompt and token input normalization
happen before this call.

`yvex_backend_op_rms_norm` is the backend RMSNorm operation used by the selected
segment. It supports the current RMSNorm boundary: F32 input/output, F16 or F32
RMSNorm weights, and explicit epsilon.

The graph API grows by adding explicit op, block, layer, prefill, decode,
logits, and generation boundaries. Each new boundary should add its own report
fields, ownership rules, and failure phases.

## Token Input

`yvex_token_input_parse_explicit`, `yvex_token_input_validate_bounds`, and
`yvex_token_input_select` define the public token input boundary.

The token input object owns a bounded copied token list. It can be validated
against vocabulary facts and selected by index before graph execution.

Prompt text becomes token input through tokenizer APIs when the artifact has
executable tokenizer metadata and the runtime path supports that conversion.

Token input is the first API layer where caller-provided token sequences become
structured runtime input. It sits above artifact and tensor readiness and below
prefill state.

## Prefill State Summary

`yvex_engine_create_prefill_state` consumes a validated `yvex_token_input` and
returns a copied `yvex_prefill_state_summary`.

The summary is built by running the selected `embedding-rmsnorm` segment
independently for each token in order. It reports token count, processed
positions, segment execution count, output bytes, scratch bytes, aggregate
checksum, final-token checksum, max diff, cleanup status, and readiness fields
for the next runtime layers.

This is the first public surface where a token sequence becomes runtime state.
It gives the KV path a concrete predecessor: positions, processed token count,
per-token graph work, aggregation, cleanup behavior, and state reporting.

KV-backed transformer prefill extends this path by attaching token ranges to
session-owned KV state. Decode, logits, sampling, and generation build on top of
that same state chain as their API surfaces mature.

## Operator Integrity Report

The operator integrity report is a CLI aggregation of public report facts. It
introduces no new ownership surface.

It summarizes artifact integrity, local digest identity, registry metadata
drift, shape and dtype accounting, tensor range validation, selected embedding
readiness, materialization preflight, and graph-entry guard status for current
graph paths.

A passing report is local operator evidence for the checked artifact and
runtime path.

Together, artifact identity reports, artifact integrity reports, tensor range
reports, shape accounting, registry metadata drift reports, materialization gate
summaries, graph guard summaries, and operator integrity reports form the
artifact-integrity reporting surface. They are caller-owned reports with copied
scalar or string fields.

## Server Status Surface

`server.h` describes the public server/status boundary.

The daemon surface is useful for process health, metrics, model listing, direct
path or alias resolution, and generation availability reporting. It gives
provider-shaped code a way to observe runtime state.

Server generation, OpenAI-compatible generation, Anthropic-compatible
generation, streaming responses, tool-call handling, and provider-level session
behavior join this API documentation when the runtime generation path underneath
them exists.

## Metrics, Traces, and Profiles

The metrics, trace, and profile headers describe the reporting layer for
runtime behavior.

YVEX currently uses command-visible reports, summaries, checksums, max-diff
fields, cleanup fields, and failure phases as its main observability path. As
the engine grows into KV, decode, logits, sampling, and generation, these
headers should carry the runtime evidence needed to debug longer executions:
artifact identity, backend, memory pressure, graph phase, token range, KV state,
logits state, sampling parameters, and server request context.

The reporting model stays tied to runtime boundaries. A metric is useful when it
says which state was measured and which command or API path produced it.

## API Extension Model

Public API growth follows runtime proof.

A new public type, function, or report field should land with runtime behavior,
tests, failure behavior, ownership rules, and documentation.

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

The API keeps these stages separate. Materialization has its own ownership and
reports. Graph execution has its own ownership and reports. Logits, sampling,
generation, and provider serving get their own surfaces as the runtime reaches
them.

## Validation Expectations

API changes should be validated through the normal runtime gate:

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
tests should prove report fields, ownership behavior, cleanup behavior, and
failure phases.

Public headers are part of the contract. A header change should be treated as a
runtime surface change, not as an internal refactor.
