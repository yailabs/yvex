<!-- docs:metadata
title: YVEX Glossary
id: yvex.reference.glossary
document: reference
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Glossary

**Orientation to distinct identities, lifetimes and evidence claims.**

[Up](README.md)

## Source and compilation

| Term | Short orientation | Definition owner |
| --- | --- | --- |
| Source snapshot | Exact immutable upstream basis | [Source](../architecture/source-provenance.md) |
| Model family / target | Repeated semantics / concrete source instance | [Integration](../model-families/integration.md#family-and-target) |
| Semantic Model IR | Sealed model-semantic aggregate | [Compiler](../architecture/compiler-ir.md#logical-projection-and-transformation) |
| Native typed IR | Types, operations, effects and explicit state | [Program contract](../contracts/computational-programs.md) |
| Transformation IR | Artifact-neutral parameter derivation | [Compiler](../architecture/compiler-ir.md) |
| PEIR | Physical Execution IR: exact package terminal truth | [Artifacts](../contracts/artifacts.md) |
| Physical program | Admitted executable operations over exact parameters | [Program contract](../contracts/computational-programs.md) |

## Runtime and state

| Term | Short orientation | Definition owner |
| --- | --- | --- |
| Artifact | Authenticated physical representation | [Admission](../architecture/artifacts-admission.md) |
| Specialization | Admitted deployment implementation choices | [Deployment](../architecture/deployment-specialization.md) |
| Engine generation | Executable resource lifetime and stale boundary | [Runtime](../architecture/runtime-lifecycle.md) |
| Session | Isolated mutable execution | [State](../architecture/computational-state.md) |
| Prefix | Identity-bound snapshot of admitted state domains | [Runtime contract](../contracts/runtime.md) |
| Execution batch / worklist | Real selected rows / routed expert ordering | [Scheduling](../architecture/scheduling-resources.md) |
| E / L | Research cognitive state / unfinished deliberation | [Native state research](../research/native-computational-state.md) |

## Evidence and delivery

| Term | Short orientation | Definition owner |
| --- | --- | --- |
| Model support | Exact source/representation/deployment/evidence boundary | [Family map](../model-families/README.md) |
| Characterization | Bounded observation, with no automatic generalization | [Methodology](../evaluation/benchmarks/methodology.md) |
| Task / Task Pack | Durable selected outcome / temporary execution grouping | [Protocol](DOCUMENTATION-PROTOCOL.md#task-and-promotion-grammar) |
| Architecture Spectrum | Pressure/falsification targets, not support catalog | [Spectrum research](../research/architecture-spectrum.md) |
