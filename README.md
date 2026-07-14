# YVEX

**YVEX is a native C model-compilation and execution system for local
open-weight models, with CUDA as its first accelerated backend.** Its
architecture treats a model as a verified logical structure that can be
compiled into explicit physical variants, lowered into concrete artifacts,
bound to hardware backends and accepted only through execution evidence.

YVEX does not identify a model with a GGUF file, and it is not yet a complete
text-generation runtime. GGUF is the v0.1.0 physical lowering target. The
release target is a complete YVEX-produced DeepSeek-V4-Flash GGUF executing real
autoregressive text generation on the 128 GB NVIDIA GB10 in DGX Spark. It is
**not release-ready or currently supported**.

The current implementation verifies the exact DeepSeek source, reconstructs
its logical architecture and tensor requirements, maps every source
contribution, and streams trusted payload ranges. Transformation execution,
quantization, complete artifact emission, full runtime binding and generation
remain unimplemented, blocked or unsupported as specified below.
[`PROJECT.md`](PROJECT.md) is the sole authority for current state, tracks,
milestones, dependencies, release gates and the Active Next milestone.

Development is human-directed, repository-grounded and evidence-gated. A
reasoning LLM expands and compresses the design space; a repository coding
agent implements bounded candidates; an independent audit decides whether the
candidate may become accepted project state. Agent activity is never treated
as project progress by itself.

## Engineering Method

YVEX uses LLMs and coding agents as engineering instruments, not project
authorities. The human authority chooses the outcome, scope, priority and
trade-offs, interprets evidence and accepts or rejects results. The reasoning
LLM studies the repository, papers, specifications and upstream
implementations; distinguishes verified facts from inferences and proposals;
and compiles architectural intent into ownership and acceptance criteria. The
coding agent reads the actual implementation, chooses coherent internal APIs
inside the prescribed boundary, implements one delta and exercises its
success, refusal, failure and cleanup paths.

The implementation is still a candidate. Its commit and closure report are
audited independently against the remote diff, owner placement, real consumers,
duplicate state, tests, failure behavior and claim boundaries. A pass advances
the accepted repository; a rejection leaves the accepted state unchanged and
returns a repair delta.

```mermaid
flowchart TD
    I["Papers, specifications,<br/>upstream code and repository"] --> D["Human / reasoning-LLM<br/>design dialogue"]
    D --> C["Persistent repository<br/>contract"]
    C --> R["Bounded delivery"]
    R --> P["Coding-agent<br/>candidate patch"]
    P --> E["Executable evidence<br/>and closure assertion"]
    E --> A["Independent remote diff<br/>and implementation audit"]
    A -->|pass| S["Accepted repository state"]
    A -->|reject| F["Repair delta"]
    F --> P
    S --> N["Next design decision"]
```

Conversations are exploratory working memory; the repository is durable
project memory. A conclusion that changes later implementation must become a
contract, owner boundary, type, test, guard, reference decision or explicit
project transition. Prompts and closure reports can describe a candidate but
cannot replace the current repository.

A delivery is the intermediate representation between intent and a patch. It
combines the persistent contracts already in the repository with one bounded
delta, mandatory ownership, implementation freedom within that ownership, hard
acceptance and proportionate validation. It specifies the required after-state
without inventing a false internal API before the coding agent has inspected
the code.

The process can be written compactly as:

```text
D_n = compile(I_n, S_n, C_n, R_n)
P_n = implement(D_n, S_n)
A_n = audit(P_n, E_n, S_n)

S_(n+1) = merge(S_n, P_n)  if A_n = pass
S_(n+1) = S_n              if A_n = reject
D_(n+1) = repair(P_n, A_n) if A_n = reject
```

`I_n` is human intent, `S_n` the accepted repository state, `C_n` its
persistent contracts, `R_n` the admitted reference evidence, `D_n` the
delivery, `P_n` the candidate patch, `E_n` its executable evidence and `A_n`
the independent audit. This models the engineering process; it is not an
implemented YVEX runtime subsystem.

Progress is the accepted transition from `S_n` to `S_(n+1)` and the behavior a
real consumer can use. Token volume, session duration, diff size, file count,
generated-code volume, report length, renaming, relocation and diagnostic
output alone do not establish completion.

## What YVEX Builds

The system target preserves ownership and identity across a controlled model
lifecycle. Status labels in the diagram describe the current release path; an
arrow is an architectural dependency, not a claim that the whole chain runs.

```mermaid
flowchart TD
    S["Verified source<br/>implemented"] --> M["Logical model<br/>implemented for DeepSeek"]
    M --> T["Transformation IR<br/>active; not implemented"]
    T --> V["Physical variant<br/>planned"]
    V --> L["Physical lowering<br/>GGUF mapping exists; emission blocked"]
    L --> A["Artifact<br/>complete artifact blocked"]
    A --> R["Runtime binding<br/>unsupported"]
    R --> E["Execution evidence<br/>release path unavailable"]
```

### Verified source

The source boundary owns repository identity, pinned revision, provider
provenance, structured configuration, tokenizer facts, shard and tensor-header
inventory, payload identity, upstream payload trust and immutable source
references. A verified source is therefore more than a directory that happens
to contain files. The DeepSeek snapshot is admitted through one canonical
inventory and one trusted payload session; later consumers do not reopen
headers or invent paths and offsets.

### Logical model

The logical model reconstructs model meaning independently from any physical
container. It owns architecture topology, canonical tensor roles and shapes,
layer structure, attention and position behavior, MoE and residual structure,
tokenizer requirements, output semantics and execution requirements.

It does not own GGUF qtype IDs, offsets or alignment; artifact paths or file
descriptors; CUDA addresses; runtime buffers; or measured execution evidence.
Those facts belong to later identities and owners.

## Model Identity Is Not Artifact Identity

```text
logical_model_id != physical_variant_id != artifact_id
```

One logical model may admit several physical variants by changing role-specific
precision, storage representation, tensor layout, expert aggregation layout,
placement, hardware profile, workload profile or numeric acceptance bounds.
These are architectural possibilities, not complete variants that YVEX emits
today; changing a physical choice does not by itself change the logical model.

An artifact is the concrete serialized output of lowering and emission. Its
existence does not prove complete tensor coverage, runtime compatibility,
generation support, numerical correctness, backend correctness, evaluation or
benchmark readiness. YVEX therefore distinguishes:

- a **tensor proof artifact**, containing one tensor or a bounded subset;
- a **complete model artifact**, containing every tensor and metadata item
  required to execute one exact model;
- a **supported model artifact**, which additionally passes integrity,
  materialization, runtime, generation, evaluation, benchmark and release
  gates.

Runtime binding separately associates an artifact with materialized views,
placement, backend operations, descriptors, persistent state and cleanup.
Execution evidence records what actually ran under an exact source, variant,
artifact, machine and workload. A plan cannot substitute for an artifact, an
artifact cannot substitute for runtime execution, and a primitive comparison
cannot substitute for transformer execution.

## Compilation Architecture

### Transformation IR

The Transformation IR is the active implementation boundary and is **not yet
implemented**. It will be the typed, artifact-neutral owner of transformations
between verified source representation and a selected physical variant.
Representative operation classes include `source reference`, `decode`, `cast`,
`reshape`, `transpose`, `concatenate`, `stack`, `aggregate`, `requantize` and
`validate`.

Each transformation must eventually record exact source dependencies, the
logical destination tensor, operation kind, input and output dtype and shape,
axis and layout semantics, precision policy, numeric acceptance requirements,
deterministic ordering and derivation identity. GGUF names, qtype IDs, offsets
and alignment remain outside this layer.

### Physical variant and physical lowering

A physical variant is one identified selection of precision by tensor role,
storage representation, tensor and expert layout, placement constraints,
hardware profile, workload profile and numeric bounds. Physical lowering then
projects an accepted variant into a concrete artifact-format contract.

For v0.1.0 that contract is:

```text
release model target: DeepSeek-V4-Flash
artifact format target: GGUF
hardware target: NVIDIA DGX Spark / CUDA
```

The GGUF lowering owns format-specific tensor names, qtype IDs, metadata keys,
directory order, byte geometry, alignment, encoded payload sizes and container
layout. Those facts do not flow backward into logical model identity. The
existing 69,187-to-1,360 DeepSeek map is concrete GGUF lowering evidence; it is
not the missing artifact-neutral Transformation IR and emits no payload bytes.

### Planning plane and byte-execution plane

Compilation separates metadata reasoning from movement of model bytes. The
planning plane decides what each output means and how it is derived. The
byte-execution plane later realizes that immutable plan through bounded source
reads, conversion, quantization and serialization.

```mermaid
flowchart TD
    subgraph P["Planning plane — immutable facts"]
        direction TB
        LM["Logical model"] --> TR["Transformation requirements"]
        TR --> PV["Physical variant"]
        PV --> LP["Lowering plan"]
    end

    subgraph B["Byte-execution plane — owned mutable resources"]
        direction TB
        PS["Trusted source payload"] --> BC["Bounded reads"]
        BC --> TX["Transformation execution"]
        TX --> PE["Physical payload production"]
        PE --> AW["Atomic artifact emission"]
    end

    TR -.-> TX
    LP -.-> AW
```

Planning is deterministic, inspectable, hashable and independent from mutable
byte-processing state. Byte execution owns buffers, streaming, numeric
conversion, encoding, cancellation, cleanup and atomic publication. Planning
never reads tensor payload bytes; source readers never reinterpret family roles,
aggregation axes or scaling companions; and quantization must consume the
canonical transformation plan rather than rediscover semantics.

Only trusted source payload streaming is implemented in the byte-execution
plane today. It is a build-time compilation input, not transformation execution
and not inference-time SSD expert streaming.

### Compact model

```math
\begin{aligned}
P   &= \Pi(M; C_p, C_h, C_w) \\
V   &= \mathcal{T}(S, P) \\
A_F &= \mathcal{E}_F(V) \\
B_H &= \mathcal{B}_H(A_F) \\
E   &= \mathcal{O}\left(\mathcal{R}(B_H, X)\right)
\end{aligned}
```

Here `S` is verified source payload, `M` the logical model, `C_p`, `C_h` and
`C_w` the precision, hardware and workload constraints, `P` the Transformation
IR, `V` the physical variant, `A_F` the artifact in format `F`, `B_H` its
runtime binding to hardware `H`, `X` runtime input and `E` execution evidence.
These are ownership equations, not claims that each operation is executable.
Precision, layout, placement or format may change every identity after `M`
without changing logical model identity.

The operators denote planning, transformation, format-specific emission,
hardware binding, execution and observation, respectively.

Future selection may be expressed as a multi-objective problem:

```math
V^{*} \in \mathcal{F}\left(\varepsilon(V), m(V), \ell(V), e(V)\right)
```

Here `F` is the admitted Pareto front; `epsilon`, `m`, `l` and `e` denote
numeric error, peak memory, latency and energy.
Constraint solving, measurement feedback, hardware/workload-aware selection,
Pareto-front selection and adaptive recompilation are future compilation lanes.
No optimizer, automatic selector or measured objective is implemented by the
current architecture contract.

Memory remains part of variant admission. Source storage, emitted artifacts,
host staging, unified or device residency, persistent KV and temporary scratch
have different owners and lifetimes. Checkpoint size alone is not a residency
plan, and a qtype with known byte geometry does not imply that a quantizer or
backend kernel exists.

## End-to-End Target and Current State

The complete architecture target is:

```text
verified source
  -> logical model
  -> Transformation IR
  -> physical variant
  -> GGUF lowering
  -> artifact
  -> materialization
  -> runtime descriptor
  -> transformer execution
  -> KV / prefill / decode
  -> logits / sampling / tokenizer
  -> text
```

The target chain does not currently execute. This public status surface is a
compact snapshot, not a second roadmap; [`PROJECT.md`](PROJECT.md) remains the
live authority.

| Boundary | Current state |
| --- | --- |
| Exact DeepSeek source identity | complete |
| 46-shard header and payload trust | complete |
| Typed DeepSeek architecture IR | complete |
| 69,187-tensor requirement coverage | complete |
| 69,187-to-1,360 logical GGUF mapping | complete |
| Bounded trusted payload streaming | complete |
| Model-compilation architecture | complete — contract defined |
| Transformation IR | active — not implemented |
| Quantization and reference dequantization | blocked |
| GGUF writer and complete artifact | blocked |
| Full materialization | unsupported |
| DeepSeek runtime descriptor and binding | unsupported |
| Full transformer execution | unsupported |
| Autoregressive text generation | unsupported |
| Evaluation | unavailable |
| Benchmark | not-measured; benchmark results are not measured |

## Current Release Target and Verified Evidence

The v0.1.0 target is:

```text
DeepSeek-V4-Flash
  -> complete YVEX-produced GGUF
  -> NVIDIA DGX Spark / CUDA
  -> real autoregressive text generation
```

[DeepSeek-V4-Flash](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash) is the
sole v0.1.0 release target, not a currently supported generation target and not
the architecture of the common engine. Qwen and Gemma remain engineering
evidence at their recorded source, profile, mapping or bounded-proof stages;
neither is a supported generation target. GLM remains planned with no canonical
implemented target contract.

The current exact DeepSeek evidence is:

| Fact | Verified value |
| --- | ---: |
| Pinned source revision | `60d8d70770c6776ff598c94bb586a859a38244f1` |
| Source shards admitted | 46 / 46 |
| Source tensors indexed | 69,187 |
| Upstream-verified shard bytes | 159,617,149,040 |
| Mapped source contributions | 69,187 |
| Logical GGUF descriptors | 1,360 |
| Pinned-standard descriptors | 1,328 |
| YVEX MTP extension descriptors | 32 |
| Complete payload passes | 1 |
| Short reads | 0 |
| Digest mismatches | 0 |
| Identity drift | 0 |

The shard-byte count is the payload covered by authoritative upstream Git LFS
SHA-256 values. It is not a local-directory footprint or a claim about emitted
artifact size. The payload pass executed the trusted source reader; it performed
no decoding, conversion, quantization or GGUF emission.

## Vertical-First Generalization

YVEX does not begin with a speculative universal model framework. It closes one
exact vertical, observes which constraints survive implementation, extracts
only those invariants into common owners and then subjects them to another
family:

```text
exact vertical
  -> implementation pressure
  -> observed invariant
  -> common owner
  -> second-family pressure
  -> harden or split
  -> preserved working verticals
```

DeepSeek-V4-Flash is the release vertical. Its compressed attention, mHC
residual topology, hash and learned routing, low-precision companions, 256-way
expert layout and memory pressure force concrete architecture decisions. They
do not authorize DeepSeek-name branches inside common source, qtype, GGUF,
artifact, residency or backend owners. Qwen and Gemma act as falsifiers of
assumptions about attention state, output tying, dense versus routed FFNs and
common tensor semantics even though neither is a v0.1.0 generation target.

A mechanism may be common from its first consumer when its contract is
intrinsically format-, storage-, arithmetic-, lifecycle- or backend-general.
Model semantics remain family-specific until repeated evidence establishes an
invariant. When another family contradicts the abstraction, YVEX strengthens
the contract or splits ownership at the semantic boundary; it does not hide the
contradiction behind a target-name conditional or break an accepted vertical.

The operating method is visible in the current architecture:

| Method rule | Concrete YVEX consequence |
| --- | --- |
| Repository identity before interpretation | The DeepSeek repository, revision, index, tokenizer, 46 shards, 69,187 tensor records and payload digests are admitted before model semantics consume them. |
| Semantics before representation | Architecture IR and exact tensor coverage define the logical model before the concrete GGUF lowering assigns format names and storage facts. |
| Evidence before capability | Row-aware qtype geometry, the file-backed GGUF reader and global layout validator close container properties without claiming a complete model artifact. |
| Fail closed at the physical boundary | CUDA context, memory, generated bundle, resolved function and exact primitive variant are separate states; absent evidence refuses rather than projecting generic readiness. |
| Second-family pressure | Qwen and Gemma evidence remains active so common owners cannot silently collapse into the DeepSeek release path. |
| Implementation as architectural learning | Payload streaming showed that byte delivery and transformation semantics require distinct owners, which introduced the artifact-neutral Transformation IR boundary before quantization. |

Primary model sources and papers define semantics; specifications define
formats and ABIs; mature repositories provide comparative implementation
evidence; hardware documentation defines physical constraints. None transfers
its API, runtime topology, support matrix, benchmark or claims to YVEX. Only
YVEX tests and identity-bound measurements determine YVEX capability.

When implementation exposes a false assumption, missing constraint, misplaced
owner or invalid abstraction, the result must return to a persistent contract,
test, guard or project decision. A lesson left only in design dialogue or an
agent closure report is not durable architecture.

## Build and Validation

The repository builds the native library, CLI and daemon. These commands are
current at this baseline:

```sh
make -j4
make check
make smoke
make check-docs
make check-cuda
```

`make check-cuda` requires a CUDA-capable host. It validates the bounded CUDA
capabilities that exist; it does not execute the DeepSeek transformer. Operator
procedures for implemented source, artifact and diagnostic boundaries live in
[`docs/operator-runbook.md`](docs/operator-runbook.md).

## Repository Orientation

| Area | Canonical owner | Responsibility |
| --- | --- | --- |
| Verified source | `src/source/` | Provenance, manifests, inventories, payload trust, immutable ranges and bounded delivery. |
| Logical model | `src/model/architecture/`, `src/model/target/` | Typed family architecture, exact tensor requirements and target-specific mapping evidence. |
| Compilation | Compilation track in [`PROJECT.md`](PROJECT.md) | Planned Transformation IR and physical-variant identity; executable owner code is not installed yet. |
| Physical lowering and artifacts | `src/gguf/`, `src/artifact/`, `src/model/artifacts/` | GGUF ABI and geometry, concrete lowering, read-only artifact admission and artifact lifecycle. |
| Materialization and runtime | `src/model/`, `src/runtime/`, `src/generation/` | Tensor ownership, runtime coordination and later model-backed state transitions. |
| Graph and backend | `src/graph/`, `src/backend/` | Graph facts, memory/execution plans, backend admission and bounded primitives. |
| Tests | `tests/` | Unit, fixture, CLI, live-source, refusal, lifecycle and guard evidence. |
| Documentation | `PROJECT.md`, `MODEL_ARTIFACTS.md`, `docs/` | Project control and non-overlapping technical contracts. |

[`docs/system-target.md`](docs/system-target.md) is the complete filesystem and
module ownership map.

## Project and Technical Documentation

| Document | Authority |
| --- | --- |
| [`PROJECT.md`](PROJECT.md) | Current state, tracks, milestones, dependencies, release gates and Active Next. |
| [`MODEL_ARTIFACTS.md`](MODEL_ARTIFACTS.md) | Artifact terminology, identity, admission and support boundaries. |
| [`docs/contract.md`](docs/contract.md) | Lifecycle, ownership, failure and implemented behavior contracts. |
| [`docs/api.md`](docs/api.md) | Public C APIs, typed results and lifetime boundaries. |
| [`docs/reference-architecture.md`](docs/reference-architecture.md) | Pinned external research, specifications, implementations and YVEX owner mapping. |
| [`docs/model-families.md`](docs/model-families.md) | Family integration and architecture semantics. |
| [`docs/operator-runbook.md`](docs/operator-runbook.md) | Executable procedures for currently implemented operator boundaries. |

External references inform implementation and independent comparison; they do
not confer compatibility, model support, backend support or performance claims.

## License

YVEX is licensed under the MIT license.
