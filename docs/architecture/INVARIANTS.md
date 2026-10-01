<!-- docs:metadata
title: Architecture Invariants
id: yvex.architecture.invariants
document: architecture
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Architecture Invariants

**Rules that survive implementation and model-family changes.**

[Up](README.md)

These invariants constrain changes. Plane dossiers explain mechanisms;
contracts specify exact interfaces; evidence determines which claims are earned.

| Invariant | Why / forbidden substitution | Deep owner |
| --- | --- | --- |
| Exact source precedes support | Model name, path or timestamp cannot authenticate bytes | [Source](source-provenance.md) |
| Logical model ≠ representation ≠ artifact | A container must not redefine model semantics | [Representation](representation-artifacts.md) |
| Artifact ≠ deployment ≠ engine | Hardware choices and mutable lifetime cannot silently alter package identity | [Deployment](deployment-specialization.md) |
| Engine generation ≠ session | Shared immutable weights cannot become shared mutable sequence state | [Runtime](runtime-lifecycle.md) |
| Runtime consumes sealed compiled facts | No family-name topology reconstruction in serving | [Compiler](compiler-ir.md) |
| Families own irreducible semantics | No family-local generic loader, scheduler or state manager | [Integration](../model-families/integration.md) |
| Transactions publish all participants or abort | Candidate state cannot leak into committed continuation | [State](computational-state.md) |
| Stale identities refuse | Reused storage cannot validate an expired borrowed result | [Runtime contract](../contracts/runtime.md) |
| Explicit backend requests never silently change backend | Exact requests, including CUDA and Metal, refuse missing admitted semantics | [Backend](backend-execution.md) |
| Fallback stays inside admitted equivalence | Integrity or missing mandatory semantics cannot be “recovered” by approximation | [Deployment](deployment-specialization.md) |
| Real populations determine work | Duplicated activations cannot manufacture semantic batch width | [Scheduler](scheduling-resources.md) |
| Evidence observes, never controls execution | Trace depth cannot change numerical behavior | [Measurement](../evaluation/measurement.md) |
| Computational state is not YAI semantic memory | Scores, KV or E cannot acquire Case/decision authority | [Interfaces](interfaces-protocols.md) |
| Promotion needs the appropriate evidence | Parser, component and release claims are not interchangeable | [Evaluation](../evaluation/README.md) |

## ABI, identity and ownership constraints

One public schema/version denotes one layout and semantic contract. Reject stale
layouts before reading added fields; change a wire version only for a wire
change. Hash/persist facts needed across lifetimes, never object memory,
padding, pointers, incidental local paths or timestamps. Within authenticated
engine generations, compact handles may retain recoverable lineage.

Production membership belongs only to [source_owners.tsv](../../config/source_owners.tsv).
The [source ownership guide](../guides/source-ownership.md) and
[C policy](../../config/c_policy.json) constrain placement and size.
Public headers are self-contained; internal headers cannot import source-private
headers. Lower layers do not depend on CLI or tests. No duplicate global owner
or include cycle may substitute for an explicit shared ABI.

## Change and evidence

If code contradicts an invariant, investigate the exact owner and tests before
rewriting either side. An unresolved ownership contradiction blocks the affected
Task; ordinary editorial choices do not. Structural selections use the
[ADR system](../decisions/README.md).
