<!-- docs:metadata
title: Deployment and Specialization
id: yvex.architecture.deployment-specialization
document: architecture-plane
status: mixed
owner: runtime
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: deployment-specialization
related: [yvex.architecture.artifacts-admission, yvex.architecture.backend-execution]
-->

# Deployment and Specialization

**Admit an implementation and resource envelope without changing artifact identity.**

[Up](README.md)

One artifact may admit several deployments. A deployment binds real hardware,
workload requirements, numerical classes and compatible physical implementations.
The resulting engine owns executable resources for that admitted choice.

## Three decisions, three owners

1. **Compiler/package:** legal operations, parameter representation and numerical obligations.
2. **Deployment:** implementation class, compatibility and resource budget.
3. **Backend:** equivalent launch geometry, buffers and synchronization inside that class.

Live free memory is an admission observation, not a new artifact identity.
Capacity distinguishes configured limits, semantic context and real available
resources. A declared context maximum is not a performance guarantee.

**Illustrative:** selecting `cuda` must refuse if that exact operation has no
admitted CUDA implementation. An `auto` policy may choose only an already
admitted equivalent; it cannot silently alter an exact request.

## Runtime binding

The runtime binding is a separate content-addressed package object. It bridges
the admitted artifact and package physical decisions to runtime descriptors,
physical tensor locations, compiled model/operator plans, tokenizer policy,
numerical identities, and compatibility constraints. It does not serialize a
selected machine, resident population, kernel cache, request shape, or backend
microkernel. The warm runtime reopens and authenticates these admitted package
facts rather than rebuilding compiler plans.

Context has two authorities at this boundary. The Semantic Model IR owns the
source-authored maximum. The immutable compiled model plan projects that fact
through target and optional draft transformer plans as a typed context
envelope. A selected startup or request capacity is instead a workload fact:
runtime may admit it only inside the compiled envelope, then the generic
capacity planner evaluates state geometry, artifact bytes, hardware facts and
resource reserve. A deployment-selected context profile is one such workload
fact, not the model's semantic limit. The public capacity contract reports the
exact loaded deployment rather than projecting a repository default as
execution truth.

Runtime binding v17 persists the canonical operator graph identity, Physical
Execution IR v5 package records, and pointer-free compiled tokenizer,
conversation, and model/operator plans. Source-owned syntax and exact tokenizer
component identities enter through the family compiler adapter; tokenizer,
runtime, and server consume the authenticated record without enumerating a
concrete family. Version 17 makes attention presence explicit so a verified
pure-SSM program can authenticate absence rather than fabricate an empty
attention plan.

The v17 reader also authenticates accepted v14/v15/v16 bindings. It imports a v14
physical record only when the legacy record names canonical package storage and
does not require its retired derived-layout/runtime-policy fields; the importer
then normalizes that package truth to PEIR v5 before engine specialization.
Unsupported legacy derived assets fail closed. Bindings v7 through v13 remain
explicit rebuild boundaries because they predate the canonical operator graph.
Old bytes are never reinterpreted as v17.

The non-persisted runtime execution profile binds an exact engine generation
and specialization to workload, kernel bundle, generation mode, evidence class,
and typed operation resolutions. One runtime-specialization owner derives it
inside the opened engine/session lifetime from the authenticated binding and
current backend facts. Generation and other output runners request workloads;
they do not promote attention, MoE or sampling capability. CUDA profiles bind
the CUDA build identity and distinguish admitted full-graph/native execution
from compatible eager/degraded execution. Consumers re-admit the sealed profile
against the current engine and backend and refuse stale, malformed or stronger
claims. It is not a second permanent execution plan.

The non-persisted implementation catalog admits prompt-position widths
separately from source-authored speculative verification widths. Catalog schema
v2 seals both policies into specialization identity; a stale schema or malformed
mask refuses before dispatch. The current generic prompt implementation admits
at most 1,024 real positions per physical batch, independently of verification
and cross-sequence scheduling. This is an implementation envelope, not a model
context limit. Routed gate/up and down consumers must agree; arena planning
reserves their admitted physical population before execution. Wider prompt work
does not widen a draft proposal or its target-verification population.
The exact selected catalog mask, configured chunk and capacity admission may
choose a smaller width. A configured chunk alone is not evidence that every
operator executes all positions together.

Durable generation checkpoints bind a plan-compatibility projection rather
than the current engine-generation profile identity. After engine reopen, the
new generation derives and admits its own exact profile before restoring the
compatible sampler/checkpoint state; a stale profile itself is never reused.

Artifact drift, binding drift, unsupported qtypes, missing roles, resource
overflow, or incompatible runtime requirements refuse before model execution.

The registry's `tensor-program` deployment class authenticates a tensor binding
rather than reading it as a generative GGUF binding. Its source digest, optional
source extent and input-position bound must agree with that binding. It retains
`finite-decision` capability and `not-applicable` execution strategy through
catalog and Host admission. The current tensor-source provider is CPU-only;
compatibility does not promote another backend or model/input grammar. Real
engine open revalidates source bytes and acquires the bounded physical stage.


## Public computational identity and physical realization

Public logical model/source and immutable package identities are independent of
machine addresses and Host lifetimes. A deployment profile selects an admitted
executable realization; the current realization is one Engine generation in one
Host. A shared inference endpoint routes the logical served model identity to
that realization. YAI Provider/Case consumers need not own its physical topology.
Several independent management connections are several observed Hosts, not proof
of a coordinated distributed deployment. Likewise several Engines in one Host
are not a distributed model.

The current public Session intentionally binds one exact Host instance and Engine
generation. That is a current execution contract, not a claim that arbitrary future
Sessions already support several physical Engines. Distributed execution requires
the runtime/compiler owners to publish logical realization identity, participant
placements, state/transaction ownership, routing and partial-failure/recovery
semantics before clients can consume a distributed realization. An endpoint or
client-local aggregation cannot supply that authority. Existing physical Session
fences must not be weakened in anticipation of this work.

The preparation assessment projects current recipe selection through public
management; it does not synthesize compiler search or topology in the client.
The OPEN Physical Model Compiler search and distributed runtime boundaries own
hardware/workload/quality-aware candidates and topology admission respectively.
Native adaptation follows [ADR 0013](../decisions/0013-native-model-adaptation-horizon.md):
derived source/package lineage reuses this deployment path, while Dataset, optimizer,
checkpoint and evaluation owners must first exist. No speculative Training or
multi-host public DTO is advertised by this refoundation.

## Implementation and evidence

[src/deployment](../../src/deployment) · [include/yvex/internal/deployment.h](../../include/yvex/internal/deployment.h) · [include/yvex/internal/deployment_compatibility.h](../../include/yvex/internal/deployment_compatibility.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Artifact and Package Admission](artifacts-admission.md) · [Backend and Device Execution](backend-execution.md)
