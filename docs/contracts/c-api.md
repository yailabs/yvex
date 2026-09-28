<!-- docs:metadata
title: C API Contract
id: yvex.contracts.c-api
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# C API Contract

**Installed public records and integration seams, with explicit internal tiers.**

[Up](README.md)

Use installed headers for public consumers. Source compatibility, wire
compatibility and internal transient schemas are distinct promises.

[Runtime ABI](runtime-abi.md) · [Numerical ABI](numerical-abi.md) ·
[Benchmark publication](benchmark-publication.md)


## Header Tiers

External consumers may include the convenience umbrella:

```c
#include <yvex/api.h>
```

Production YVEX code includes the exact domain header it consumes. The umbrella
contains these installed domain headers:

| Header | Stable domain |
| --- | --- |
| `<yvex/core.h>` | status, bounded errors, paths, logging, version identity |
| `<yvex/source.h>` | source provenance, accounts, manifests, native tensor inventory |
| `<yvex/gguf.h>` | bounded GGUF v3 parsing, metadata, tensor directory, layout facts |
| `<yvex/artifact.h>` | immutable file snapshots, identity, integrity and admission |
| `<yvex/model.h>` | artifact-neutral dtypes, tensor roles and model descriptors |
| `<yvex/catalog.h>` | remote-provider, acquired-source and local-package catalog records |
| `<yvex/materialization.h>` | backend-owned materialized-weight lifecycle and views |
| `<yvex/qtype.h>` | canonical GGUF qtype identity and storage geometry |
| `<yvex/quant.h>` | quantization policy, job and calibration manifests |
| `<yvex/graph.h>` | generic graph, planning and memory-plan contracts |
| `<yvex/backend.h>` | backend admission, device tensors and primitive dispatch |
| `<yvex/provider.h>` | transport-neutral application request and result semantics |
| `<yvex/tokenizer.h>` | tokenizer views, tokenization and prompt rendering |
| `<yvex/registry.h>` | local model registry and typed reference resolution |
| `<yvex/server.h>` | local protocol, runtime host, sessions, telemetry and thin client lifecycle |
| `<yvex/finite_decision.h>` | bounded token-domain finite-decision engine and uncalibrated typed computational result |
| `<yvex/finite_decision_producer.h>` | bounded semantic-neutral question/frontier request and identity-bound local-process result; model input construction stays inside YVEX |
| `<yvex/server_finite_decision.h>` | typed local C execution against an exact resident finite-decision engine generation |

Headers below `include/yvex/internal/` are non-installed cross-subsystem ABI.
They are available to repository production owners and focused tests only;
`<yvex/api.h>` never includes them. No source-local header is part of either
surface.

The finite-decision producer request is a local, versioned computational
contract: exact model alias and generation, bounded question/context text, and
an ordered finite population of opaque candidate IDs with text representations.
The caller supplies no token IDs, model type IDs, templates or marker positions.
The admitted input policy constructs those facts behind the host boundary.
The result names raw model logits, a relative distribution over precisely that
population (not calibrated confidence), source/model/binding/tokenizer/program,
input-policy and execution identities, plus zero-generation/resource evidence.
The existing direct `<yvex/finite_decision.h>` token-domain API is unchanged.

The common-runtime cutover intentionally retired the former installed
`runtime.h`, `generation.h`, and `metrics.h` diagnostic contracts. Those
headers exposed a bounded proof engine, flat F32 KV, fixture logits/sampling,
and report-only metrics; they were not a model-backed runtime ABI. Retaining
them would preserve a second lifecycle beside the sealed runtime model and
session. Current KV, generation, and observability contracts are admitted
through explicit internal owners and the installed server/protocol boundary
rather than compatibility headers.

All public headers are independently includable in C and C++. Public option
structures borrow pointer fields for the duration of a call. An opaque object
returned through an output pointer is caller-owned until its matching close or
release function runs.


## Installed ABI Versioning

A versioned installed record is identified by its C type and schema value.
One such pair names one field layout and semantic contract. A binary layout or
incompatible semantic change advances that record's schema; it does not advance
the private local protocol unless the wire contract also changes. Internal
records rebuilt with the binary do not acquire public schema versions merely
because their implementation changes.

`yvex_server_options` schema v5 is the current host-options contract.
The loader callback receives the requested per-engine context capacity; the
host still separates its configured engine capacity from actual resource
admission. Earlier option schemas refuse before fields or callback layouts
are interpreted. Repository callers use `YVEX_SERVER_OPTIONS_SCHEMA_CURRENT`.
This installed record and the private wire version have separate identities.

`yvex_tokenizer_plan_summary` schema v5 identifies prompt composition with
family-neutral `conversation` and `verbatim` values, separates tokenizer
architecture from pre-tokenizer behavior, and carries source-authored reasoning
capabilities/defaults. Schema v3 and v4 remain historical identities; newly
sealed plans publish v5. `yvex_prompt_message` schema v1 makes typed assistant
reasoning history an explicit, rejectable input layout rather than an unchecked
extension of the earlier pre-v0.1 prompt record.

The installed catalog split introduced unversioned pre-v0.1 source/API
migrations rather than assigning an existing schema identity to new layouts:
remote records, local source records, local package records, and live engine
observations now have distinct owners. Graph and materialization declarations
moved to their installed domain headers without changing their layouts. The
public ABI guard binds every explicitly versioned installed record to a
comment-insensitive declaration signature and compiler-checked 64-bit C/C++
layout facts. Changing one requires an explicit schema and migration decision.


## Status, Failure, And Publication

Public functions return `YVEX_OK` on success and a typed status otherwise.
Functions accepting `yvex_error *` write bounded copied context; the message
does not borrow parser, backend or stack storage.

Failure remains attached to its owning boundary. Parse refusal, artifact drift,
materialization failure, backend refusal, execution failure and cleanup failure
are not interchangeable. A renderer may project the code and context, but it
does not classify capability from error text.

Transactional owners publish only complete state. Artifact and manifest
writers use no-replace atomic publication. Graph and runtime execution produce
candidate output and state first, then commit them together. Cancellation or
failure leaves the previous committed state unchanged.


## Artifact And GGUF Ownership

`yvex_artifact_open` retains one read-only file handle and immutable snapshot
until `yvex_artifact_close`. With mapping disabled, callers use bounded
positioned reads; an optional read-only mapping is valid only for the handle
lifetime.

`yvex_gguf_open_ex` borrows an artifact during construction and then owns its
decoded metadata, names, arrays, tensor directory and indexes. Structural parse
does not read tensor payload bytes. `yvex_gguf_layout_validate` checks canonical
directory order, qtype-sized ranges, alignment, zero padding, total span and
snapshot stability without promoting the file to a complete artifact.

Qtype geometry comes only from `<yvex/qtype.h>`. Storage admission uses
`dims[0]` as row width and checks block divisibility and every multiplication.
Storage geometry, decoding, encoding, CPU compute, CUDA compute and runtime
support remain separate facts.

Complete-artifact admission under `<yvex/artifact.h>` binds physical structure,
required metadata, tokenizer evidence, tensor inventory and exact file
identity. The admitted file remains external operator data. A complete
artifact is still not a supported artifact.


## Model Registry And Startup Profiles

`<yvex/registry.h>` owns the local model catalog and typed reference
resolution. Registry schema `yvex.models.local.v7` preserves artifact entries and optional
typed startup profiles. Its `working_set` array records canonical logical-model
identities independently of local payload residency. Artifact-only entries do
not create a startup profile merely by naming a runtime target. Every profile records engine kind independently from
execution strategy. `single-artifact` text profiles carry the absolute
artifact, exact runtime-binding path, runtime target, backend, semantic
`target-only` or `speculative` strategy, and positive context capacity.
`composite` media profiles carry an installed component root, target and
backend with a `not-applicable` execution strategy; they do not manufacture a
singular artifact or runtime binding. Older v1 through v6 catalogs remain
readable. The importer maps the former `target-only`, `dspark`, and `media`
mode values onto the two current axes; current writers never emit the mixed
legacy mode.

The installed in-process `yvex_model_registry_entry` contract is explicitly
versioned at schema v1. Callers set `schema_version` to
`YVEX_MODEL_REGISTRY_ENTRY_SCHEMA_CURRENT`; mutation and startup validation
reject any other value before reading the remaining fields. The former
unversioned layout is not a binary compatibility surface. Model-library
projections expose the separately versioned
`yvex_model_runtime_profile_fact` v2 record.

`yvex_model_library_matches` resolves exact catalog names, including original
registry model names retained when logical entries are aggregated. The names
belong to the snapshot and disappear at close; they do not duplicate models or
select deployments. Callers reject matches across multiple logical models.

`yvex_model_registry_startup_validate` checks the facts required by the profile
kind and the corresponding local file or installation accessibility. It does
not authenticate identities, materialize weights, initialize a backend, or
establish runtime support. After the persistent host is running,
the engine-load operation performs full singular or composite admission and
publishes one new engine generation. The TTY CLI selects a profile
interactively; API and noninteractive clients always supply its exact identity.

A composite media target follows the same readiness rule. Before publishing
`READY`, the server opens the tokenizer and every component artifact, reconciles
their typed admissions, and retains those immutable views under one runtime-model
identity. Payload materialization and CUDA residency remain separate facts: a
component set larger than the device's memory envelope is staged at execution
phase boundaries rather than being reported as simultaneously resident.

The default catalog is user-local data at
`~/.local/share/yvex/models.local.json`; an explicit `YVEX_DATA_DIR` changes
that owner for controlled deployments. Catalog entry, invocation-selected
profile, and live runtime model are three distinct facts.


## Model, Materialization, And Backend

`<yvex/model.h>` exposes canonical dtype, tensor-role and model-descriptor
facts. `<yvex/materialization.h>` separately owns materialized-weight objects
that describe bounded backend-owned storage; their presence does not imply
complete runtime-model residency.

`<yvex/catalog.h>` keeps provider records, acquired local sources, and admitted
local packages as separate record types. A CLI or future GUI may join those
records with live engine observations, but no catalog record acquires server or
engine authority.

`<yvex/backend.h>` exposes backend discovery, capability facts, device tensor
lifecycle and admitted primitives. Backend code consumes typed operations and
does not infer family topology. `yvex_backend_close_checked` nulls its owner
only after complete discharge and retains it when cleanup must be retried;
`yvex_backend_close` remains the best-effort compatibility projection for
callers without a failure channel.

The concrete backend object and dispatch table are source-local backend ABI.
Graph and runtime owners hold an opaque `yvex_backend` and use typed operations
for allocation, transfer, capability queries, residency mappings and workspace
access. In particular, session-state publication advances the backend-visible
residency generation through a checked operation; callers cannot mutate backend
dispatch, placement or generation fields directly.

The generated CUDA bundle, Driver API module/function resolution and CUDA Graph
objects are repository-internal backend contracts. They are not installed C
ABI and they do not imply a model-generation path.

Kernel-bundle identity v3 binds the selected image class, target architecture,
ordered module count, and exact bytes of every manifest-owned CUDA module. An
`sm_121` build embeds both portable PTX and native CUBIN for each independently
compiled kernel family; capability 12.1 selects the complete CUBIN set, while
other devices retain the complete explicit PTX set. Admission loads every
module and resolves every required function before publishing any capability.
The existing non-installed CUDA summary carries the selected image class,
architecture and aggregate identity. It does not add installed API.


## Artifact-Bound Tokenizer Runtime

`<yvex/tokenizer.h>` exposes the exact admitted tokenizer plan, explicit-length
UTF-8 encoding, bounded source-authored conversation rendering, batch and incremental
ByteLevel decoding, special/EOS classification, and a generation-local token
append directory. Immutable vocabulary/merge/added-token indexes follow model
lifetime; incremental decoder and append contexts are isolated mutable owners.
Every owned result publishes only after complete success and carries field-wise
identities. The compiler-facing `<yvex/internal/tokenizer.h>` owns the canonical
pointer-free policy and codec; runtime instantiation never searches a family
registry. These operations do not read weights, mutate KV, append sampled tokens
to decode, or compose generation.


## Compiled Operator Registry Boundary

The operator registry is a build-time, non-installed source contract rather
than public C ABI. Strict `yvex.operator.registry.v1` JSON is validated and
projected into immutable generated C descriptors compiled into `yvex`. The
descriptors contain stable operation IDs, typed adapter enums, syntax,
visibility, requirements, and transport projections. They contain no domain
callbacks, arithmetic, resources, or protocol codecs.

The generated registry header is consumed only by product command owners; it
is not included by `<yvex/api.h>` or packaged as a mutable runtime dependency.
Domain APIs retain semantic validation and lifecycle. Runtime-client adapter
objects remain protocol-only, while finite offline adapters may consume the
non-installed engine interfaces already documented here.


## Execution Truth ABI

`<yvex/execution.h>` owns three pointer-free schema-v1 records shared by typed
runtime reports, the private protocol, and clients. Capacity separates resident
sessions, scheduler-visible runnable work, physical sequence width,
cooperative scheduling, compatible-operation batching, and dynamic continuous
batching. Measurement binds scope, clock, composition, unit, availability,
duration, explicit denominator, and independent cumulative/rolling rates.
Resource truth separates model, typed session state, arena, workspace,
transient, process, placement, current/peak, and movement facts.

The records do not expose a scheduler queue, CUDA object, family type, profiler,
or benchmark policy. Availability distinguishes unknown from measured zero;
overlapping spans and timing scopes remain non-additive. Protocol codecs reject
unknown enums, inconsistent availability, impossible rate denominators, invalid
current/peak relations, and UMA claims that confuse device addressability with
measured physical page residency.


<a id="application-provider-and-local-protocol-v22"></a>

## Application Provider and Local Protocol

`<yvex/provider.h>` is the installed transport-neutral application request and
result ABI. Provider schema v3 represents an omitted completion
limit as adaptive while binding separate assistant reasoning content,
reasoning policy, source-authored drop behavior, field-presence facts, and a
bounded ordered tool-call set in addition to the v1 request facts. Provider wire
v3 carries those fields and the adaptive limit. Provider schema/wire v4 adds a
source-default reasoning request and a separate source-default/drop/preserve
history policy, so model-family conversation policy remains authoritative when
the application omits either choice. V1 remains readable and writable only with disabled
reasoning, at most one assistant tool call, and its original field semantics.
Clone and wire-decode publish only a complete owned request graph. The provider
owner neither parses HTTP nor renders model-family prompt syntax.

`<yvex/server.h>` protocol v24 carries the sealed provider request through the
private Unix socket. Provider output messages distinguish assistant text,
explicit reasoning, function calls, usage, terminal completion, and failure.
Typed events bind the provider adapter, provider-request identity, and external
correlation ID while excluding prompt and output content.

The current wire contract is [Local Protocol v24](local-protocol.md).
It owns operation layout, negotiation, generation routing, typed content,
execution preflight, per-engine load context, finite-decision requests and
availability semantics. Every earlier wire version refuses; there is no private
pre-v0.1 compatibility decoder. Historical version transitions remain in Git
and do not constitute simultaneously supported layouts.

Provider reasoning/history policy remains source-authored. Resource and
measurement records distinguish real physical width, scheduler concurrency,
scoped/overlapping time, placement and unavailable facts. An absent observation
is not a measured zero; device addressability is not physical page residency.

Protocol error messages carry `yvex_client_failure_class`, so adapters map
queue capacity, timeout, incompatible state and unsupported input without
inspecting diagnostic text. `yvex_client_timeout_set()` bounds application
adapter send/receive waiting; zero restores unbounded post-handshake waiting
for native watch and trace consumers.

The source-separated OpenAI adapter inside the foreground server consumes only the provider
contract, protocol client, and bounded HTTP/JSON/SSE owners. It opens no second
artifact or model, owns no KV, and cannot call Transformer, generation, or CUDA
owners directly. The exact HTTP profile is documented in
[`openai-compatibility.md`](openai-compatibility.md).


## Capability And Claim Boundary

The common runtime publishes granular facts for semantics, core/envelope,
CPU/CUDA phase and mode, residency, workspace, state delta, trace, profile and
benchmark readiness. Compatibility booleans may be derived from that lattice;
they are not independent capability authorities.

The current runtime supports the complete hosted DeepSeek prompt-to-text path on
CPU and the admitted mixed GB10 CUDA path. It composes exact tokenizer encoding,
prompt-suffix prefill, persistent state, MoE, the complete Transformer, raw
vocabulary logits, common transactional sampling with admitted CPU/CUDA
selection, sampled-token decode feedback, typed
stop, incremental detokenization, and committed streaming through one server
model and isolated server sessions. It does not establish public serving,
model evaluation, a release-path full-model benchmark, or release readiness.


## Extension Rules

New installed declarations require a stable external lifecycle and tests.
Internal implementation convenience is not a public ABI reason. A future model
family registers typed facts and sequence-mixer lowering against the common
runtime; it does not receive its own model/session implementation.

Every API extension must define:

1. owner and header tier;
2. borrowed and owned inputs;
3. success publication and failure rollback;
4. identity and invalidation dependencies;
5. cleanup behavior;
6. focused positive, refusal and lifecycle tests;
7. the exact capability boundary it does and does not promote.
