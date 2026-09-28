<!-- docs:metadata
title: Model Semantics and Admission
id: yvex.architecture.model-semantics
document: architecture-plane
status: mixed
owner: model
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: model-semantics
related: [yvex.architecture.compiler-ir, yvex.architecture.computational-state]
-->

# Model Semantics and Admission

**A family interprets a model; shared machinery owns its execution lifetimes.**

[Up](README.md)

A family is a pattern of mathematical and state semantics. A target is an exact
source instance. Neither name is an admission certificate.

## Interpretation boundary

| Family owns | Common owners consume |
| --- | --- |
| Tensor roles, source configuration and topology | Sealed model descriptors and complete role coverage |
| Attention, recurrent, component and output meaning | Typed programs and state recipes |
| Numerical obligations and source tokenizer rules | Admitted execution classes and exact input policy |

Family code does not own a second allocator, artifact reader, scheduler, session
registry or telemetry system. Model and graph adapters supply semantics through
bounded compiler sinks. Only irreducible fused backend behavior justifies a
backend family implementation.

## What support means

Source recognition → role coverage → admitted artifact → admitted deployment
→ real execution → independent qualification. Each arrow is a separate gate.
The [family integration contract](../model-families/integration.md) owns exact
admission obligations; [Status](../project-control/STATUS.md) owns the current
bounded capability posture. A family dossier records its exact evidence.

## Current family lowering

Exact source inventories, role counts, and physical-policy limitations belong
to the [family records](../model-families/integration.md#current-family-boundaries).
Shared compilation preserves source derivation and shared operands instead of
duplicating bytes for multiple plans.

The typed architecture is not yet a complete deployed decoder for every
sequence mixer. Mamba2 now has source-owned token/numerical policy, typed
pure-SSM forward/output programs, an admitted artifact/engine binding and
exact internal all-layer CPU execution. Mandatory FFN or rotary/KV assumptions
may not be satisfied with fictitious roles to manufacture support; hosted
conversation remains unavailable without a source-owned template.


## Implementation and evidence

[src/model/families](../../src/model/families) · [src/graph/families](../../src/graph/families) · [include/yvex/model.h](../../include/yvex/model.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[IR and Compilation](compiler-ir.md) · [Computational State](computational-state.md)
