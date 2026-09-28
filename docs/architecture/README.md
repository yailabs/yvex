<!-- docs:metadata
title: YVEX System Architecture
id: yvex.architecture
document: architecture
status: mixed
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX System Architecture

**Verified source, compiled meaning and isolated computational execution.**

[Documentation](../README.md)

YVEX turns exact upstream model sources into admitted computational execution.
Its compiler seals meaning and physical work; its runtime owns engine/session
lifetimes; its backends execute admitted operations. Evidence observes these
boundaries without becoming an execution owner.

## System context

<!-- docs:diagram system_overview -->
```mermaid
%% yvex-figure: system_overview
flowchart TB
  n_providers["EXTERNAL<br/>Provider sources<br/>weights · config · tokenizer"]:::external
  n_applications["EXTERNAL<br/>SDKs / applications<br/>local compatibility clients"]:::external
  n_cli["INTERFACE<br/>yvex CLI<br/>chat + host / model / session"]:::interface
  n_reports["EVIDENCE<br/>Observation clients<br/>host logs · status · evidence"]:::evidence
  subgraph n_panel_0["a  Offline lane"]
    direction TB
  n_source["SEMANTIC<br/>Source acquisition<br/>inventory · provenance · trust"]:::semantic
  n_compiler["SEMANTIC<br/>Compiler / package<br/>family lowering · physical plan"]:::semantic
  n_package["SEMANTIC<br/>Artifact + binding<br/>authenticated package facts"]:::semantic
  end
  subgraph n_panel_1["b  Persistent yvex serve"]
    direction TB
  n_http["INTERFACE<br/>HTTP adapter<br/>bounded OpenAI compatibility"]:::interface
  n_protocol["INTERFACE<br/>Typed local protocol<br/>native UDS · exact generation"]:::interface
  n_engines["RUNTIME<br/>Engine generations<br/>admitted plans and resources"]:::runtime
  n_sessions["MUTABLE<br/>Sessions / state<br/>generation-bound continuity"]:::mutable
  n_work["RUNTIME<br/>Execution scheduling<br/>ready work · real rows"]:::runtime
  n_events["EVIDENCE<br/>Typed events<br/>progress · results · resources"]:::evidence
  end
  subgraph n_panel_2["c  Backend"]
    direction TB
  n_backend["PHYSICAL<br/>CPU / CUDA backends<br/>buffers · admitted kernels · submission · synchronization"]:::physical
  end
  n_providers --> n_source
  n_source --> n_compiler
  n_compiler --> n_package
  n_applications -. request .-> n_http
  n_cli -. request .-> n_protocol
  n_http -. request .-> n_protocol
  n_protocol -. request .-> n_engines
  n_package --> n_engines
  n_engines ---|identity| n_sessions
  n_engines -. request .-> n_work
  n_work --> n_backend
  n_work -. observation .-> n_events
  n_events -. observation .-> n_reports
  n_panel_0 ~~~ n_panel_1 ~~~ n_panel_2
  classDef semantic fill:#efe5fc,stroke:#7541ba,color:#261b38
  classDef physical fill:#f4effb,stroke:#8054b2,color:#261b38
  classDef runtime fill:#eeeafb,stroke:#6a4ca3,color:#261b38
  classDef mutable fill:#fff3db,stroke:#8e6920,color:#261b38
  classDef interface fill:#edf3fb,stroke:#456789,color:#261b38
  classDef external fill:#f2f2f4,stroke:#707078,color:#261b38
  classDef evidence fill:#eaf5ef,stroke:#3d7255,color:#261b38
  style n_panel_0 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
  style n_panel_1 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
  style n_panel_2 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
```

[Static figure](../assets/diagrams/system_overview.svg) · [Editable source](../assets/diagrams/system_overview.json)
<!-- /docs:diagram -->

[Editable context source](../assets/diagrams/system_overview.json).
Operators, C consumers and application providers enter through typed interfaces.
YAI owns semantic work outside this boundary. Hardware executes the admitted
computation; a transport adapter does not confer additional model capability.

## Source to verified execution

<!-- docs:diagram physical_compilation -->
```mermaid
%% yvex-figure: physical_compilation
flowchart TB
  subgraph n_panel_0["a  Verified source and family interpretation"]
    direction TB
  n_source["EXTERNAL<br/>Verified source<br/>inventory · ranges · trust"]:::external
  n_family["SEMANTIC<br/>Family interpretation<br/>irreducible source semantics"]:::semantic
  n_semantic["SEMANTIC<br/>Semantic Model IR<br/>sealed model aggregate"]:::semantic
  end
  subgraph n_panel_1["b  Coordinated compilation — computation and parameter/package lanes"]
    direction TB
  n_typed["SEMANTIC<br/>Native typed program<br/>functions · values · state"]:::semantic
  n_execution["SEMANTIC<br/>Program Execution IR<br/>entry slots · dependencies"]:::semantic
  n_transform["SEMANTIC<br/>Transformation IR<br/>source → terminal"]:::semantic
  n_transform_binding["SEMANTIC<br/>Transform binding<br/>verified ranges + IDs"]:::semantic
  n_variant["PHYSICAL<br/>Variant / artifact<br/>qtype · rows · layout"]:::physical
  n_peir["PHYSICAL<br/>PEIR package truth<br/>authenticated terminals"]:::physical
  end
  subgraph n_panel_2["c  Identity-preserving join and immutable binding"]
    direction TB
  n_join["SEMANTIC<br/>Parameter join<br/>symbol ↔ lineage"]:::semantic
  n_physical_program["PHYSICAL<br/>Physical program<br/>admitted impls"]:::physical
  n_compiled["SEMANTIC<br/>Compiled plan<br/>programs + schedule"]:::semantic
  n_binding["INTERFACE<br/>Runtime binding<br/>authenticated truth"]:::interface
  end
  subgraph n_panel_3["d  Deployment and executable resources"]
    direction TB
  n_deployment["RUNTIME<br/>Deployment specialization<br/>real backend · device · resources"]:::runtime
  n_engine["RUNTIME<br/>Engine generation<br/>executable resource ownership"]:::runtime
  end
  n_source --> n_family
  n_family --> n_semantic
  n_semantic --> n_typed
  n_typed --> n_execution
  n_family --> n_transform
  n_transform --> n_transform_binding
  n_transform_binding --> n_variant
  n_variant --> n_peir
  n_execution ---|identity| n_join
  n_peir ---|identity| n_join
  n_join --> n_physical_program
  n_physical_program --> n_compiled
  n_compiled --> n_binding
  n_binding -->|gate| n_deployment
  n_deployment --> n_engine
  n_panel_0 ~~~ n_panel_1 ~~~ n_panel_2 ~~~ n_panel_3
  classDef semantic fill:#efe5fc,stroke:#7541ba,color:#261b38
  classDef physical fill:#f4effb,stroke:#8054b2,color:#261b38
  classDef runtime fill:#eeeafb,stroke:#6a4ca3,color:#261b38
  classDef mutable fill:#fff3db,stroke:#8e6920,color:#261b38
  classDef interface fill:#edf3fb,stroke:#456789,color:#261b38
  classDef external fill:#f2f2f4,stroke:#707078,color:#261b38
  classDef evidence fill:#eaf5ef,stroke:#3d7255,color:#261b38
  style n_panel_0 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
  style n_panel_1 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
  style n_panel_2 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
  style n_panel_3 fill:#faf8fe,stroke:#b8a5d0,color:#261b38
```

[Static figure](../assets/diagrams/physical_compilation.svg) · [Editable source](../assets/diagrams/physical_compilation.json)
<!-- /docs:diagram -->

[Editable compiler source](../assets/diagrams/physical_compilation.json).

The signature lifecycle is a **fork and join**, not a linear IR-to-GGUF conversion:

1. **Source and family interpretation** authenticate provenance and interpret model meaning.
2. **Computation lane** seals semantic/program IR and lowers legal operations.
3. **Parameter lane** derives transformations, quantization, layout and an admitted artifact.
4. **Parameter join** binds symbolic computation to exact package terminal truth.
5. **Deployment** admits numerical/implementation classes and resource requirements.
6. **Engine generation** owns immutable executable resources; **sessions** own state.
7. **Scheduler and backend** select real ready work and execute admitted operations.
8. **Result and evidence** publish only after the required identity/lifecycle checks.

## Logical planes

### Interpret and compile

- [Source and Provenance](source-provenance.md) — Authenticate the exact source before interpreting or executing it.
- [Model Semantics and Admission](model-semantics.md) — A family interprets a model; shared machinery owns its execution lifetimes.
- [IR and Compilation](compiler-ir.md) — Seal computation and parameter lineage before runtime allocation.
- [Representation, Quantization and Artifacts](representation-artifacts.md) — A logical model can have several exact physical representations.
- [Artifact and Package Admission](artifacts-admission.md) — Authenticate a complete package before publishing a usable mapping.

### Admit and execute

- [Deployment and Specialization](deployment-specialization.md) — Admit an implementation and resource envelope without changing artifact identity.
- [Engine and Session Runtime](runtime-lifecycle.md) — A host retains engines; each session owns independent mutable execution.
- [Computational State](computational-state.md) — Sessions isolate state; transactions coordinate publication across distinct geometries.
- [Scheduling and Resources](scheduling-resources.md) — Schedule real ready work and account for resources without inventing physical width.
- [Backend and Device Execution](backend-execution.md) — Execute admitted work; do not rediscover model semantics in kernels.

### Produce and integrate

- [Generation and Decoding](generation-decode.md) — Generate tokens through compiled model work and transactional publication.
- [Speculation and Finite Execution](advanced-generation.md) — Shared runtime machinery supports distinct result semantics.
- [Interfaces and Protocols](interfaces-protocols.md) — Clients project typed system behavior without acquiring execution ownership.

## State and lifetime view

| Object | Owns | Survives / expires with |
| --- | --- | --- |
| Source snapshot | Exact provenance and tensor basis | Immutable source identity |
| Logical model/program | Interpreted computation and legal effects | Compiled identity |
| Representation/artifact | Exact encoded parameters and package facts | Authenticated stored bytes |
| Deployment specialization | Admitted implementation and resource choices | Compatible hardware/workload envelope |
| Engine generation | Executable resources and stale-reference boundary | Load/unload/drain |
| Session | Mutable attention/recurrent/component state | Session lifecycle and its parent generation |
| Transaction/result borrow | Candidate publication and transient value validity | Atomic commit/abort or producer reuse |

Captured prefixes are identity-bound snapshots, not generic semantic memory.
Persistent cognitive state E and unfinished deliberation L remain
[research](../research/native-computational-state.md). A client disappearing
does not transfer or destroy host authority; cancellation and retirement follow
the [runtime contract](../contracts/runtime.md).

## Execution and generation view

The scheduler chooses ready progress. An execution batch records actual selected
rows; an expert worklist groups their routed populations. CUDA owns buffers,
submission, synchronization and equivalent launch geometry below admitted
execution. It does not infer topology from a family name or tensor dimensions.

Ordinary generation runs tokenizer/admission → prefill → decode/logits →
sampling → transactional publication. Speculation adds target verification;
finite readout supplies candidate tokens instead of sampling; native finite
models compute typed results directly. Media composition uses admitted component
programs, not a second generic runtime.

## Trust and support view

“Supported model” means a bounded combination of source, representation,
deployment, execution and evidence. Family recognition, a complete artifact,
component numerics, model behavior and release qualification are separate gates.
Missing semantics, stale generations and unsupported exact requests fail closed.

See [Invariants](INVARIANTS.md), [Contracts](../contracts/README.md),
[family evidence](../model-families/README.md) and [current Status](../project-control/STATUS.md).

## Source ownership and dependency direction

Core/public ABI → source/artifact/model/tokenizer → compiler/graph →
materialization/runtime → backend → generation → evaluation.
Domain facts flow through typed reports to renderers and CLI I/O.

## Authority boundaries

| Boundary | Current owner |
| --- | --- |
| Source provenance, inventory, payload trust | `src/source/` |
| Remote provider discovery and remote representation records | `src/accounts/`, `src/model/remote.c`, `include/yvex/catalog.h` |
| Local acquired-source and admitted-package catalogs | `src/model/catalog.c`, `src/model/artifacts/`, `include/yvex/catalog.h` |
| Family source facts, coverage and logical lowering | `src/model/families/` |
| Authenticated finite-frontier text/token construction | `src/tokenizer/finite_input.c` and source-owned recipes under `src/tokenizer/families/` |
| Artifact-neutral parameter transformation, transform binding and representation policy | `src/model/compilation/`, model compilation owners |
| GGUF container, qtypes, writer, layout | `src/gguf/` |
| Artifact snapshot, integrity, admission, bounded ranges, and package mapping/materialization session | `src/artifact/`, `include/yvex/internal/artifact.h` |
| Backend/model executable weight materialization | `src/model/materialization.c`, `include/yvex/materialization.h` |
| Semantic Model IR, native typed programs, execution lowering, physical programs and state protocols | `src/model/compilation/`, `src/ir/`, `src/graph/` |
| Runtime binding, model engines, specialization, sessions, residency, scheduler | `src/runtime/` |
| Device capability, memory, kernels, launch graphs | `src/backend/` |
| Autoregressive composition | `src/runtime/generation.c` and typed generation owners |
| Persistent host, live engine observation, routing, protocol, telemetry | `src/server/` |
| OpenAI-compatible projection | `src/server/openai/` and `src/provider/` |
| Command metadata and projections | `config/operator/registry.json`, generated descriptors, `src/cli/` |

Family source interpretation and graph recipes occupy separate compilation
levels, described by [family integration](../model-families/integration.md).
Generic runtime and backend owners consume typed plans, not family-name
switches. Pure-SSM execution now has the exact CPU boundary documented by the [Mamba2 record](../model-families/mamba2.md); this does not imply hosted conversation or CUDA SSM support.

The exact source-file ownership manifest is
[`config/source_owners.tsv`](../../config/source_owners.tsv). It is also the
sole handwritten production build-membership list; a checked deterministic
projection supplies Make product classes. Contributor-facing layout and build
ownership rules are in
[Source and Module Ownership](../guides/source-ownership.md).


## Next reads

[Invariants](INVARIANTS.md) · [Contract map](../contracts/README.md) · [Evaluation](../evaluation/README.md) · [Tasks](../project-control/TASKS.md)
