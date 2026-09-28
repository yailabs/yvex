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

Durable generation checkpoints bind a plan-compatibility projection rather
than the current engine-generation profile identity. After engine reopen, the
new generation derives and admits its own exact profile before restoring the
compatible sampler/checkpoint state; a stale profile itself is never reused.

Artifact drift, binding drift, unsupported qtypes, missing roles, resource
overflow, or incompatible runtime requirements refuse before model execution.


## Implementation and evidence

[src/deployment](../../src/deployment) · [include/yvex/internal/deployment.h](../../include/yvex/internal/deployment.h) · [include/yvex/internal/deployment_compatibility.h](../../include/yvex/internal/deployment_compatibility.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Artifact and Package Admission](artifacts-admission.md) · [Backend and Device Execution](backend-execution.md)
