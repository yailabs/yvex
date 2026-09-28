<!-- docs:metadata
title: Computational State Realization
id: yvex.research.state-realization
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Computational State Realization

**How should logical computational state map to exact compiled lifetimes?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

How should logical computational state map to exact compiled lifetimes?

Prove identity, stale-generation refusal, transaction atomicity, compatibility and invalidation under a real reference model. A serialization layout alone cannot establish state continuity.


## E generations, derived materializations and physical lifetime

| Lifecycle state | Meaning |
| --- | --- |
| E_committed | Immutable persistent generation visible at execution start. |
| E_working_g | Intra-execution trajectory, unpublished. |
| E_candidate | Final unpublished successor awaiting transaction resolution. |

```text
E_committed → begin → E_working_0 → Read / Slow Update → E_working_1 → ...
    → E_candidate → prepare ──► commit / publish → next E_committed
                           └─► abort / discard
```

Failure, cancellation or rejected work must never partially mutate committed
E. Reuse Program S's transaction philosophy; current candidate/committed
sequence transactions do not already implement E or B1 State Update.

An engine/executable model would own immutable model resources and compatible
E realization generations. Sessions/requests receive a binding/lease to an
exact committed generation. Qualified read-only bindings may share physical
backing; state-producing work creates a candidate successor rather than
mutating shared committed E in place. The realization carries identity, exact
composition and StateProfile, generation, external-source provenance,
physical representation, residency, derived materializations, checkpoint,
compatibility and invalidation state. S owns paging, sharing/COW, movement,
rollback, resource accounting and scheduling; this lifecycle may outlive a
normal request/session.

Slow-changing E permits derived State Read materializations. In B1-v0,
`K_E[layer]` and `V_E[layer]` each have shape `[64,512]`. Across 64 sites,
128 BF16 tensors require `2 × 64 × 64 × 512 × 2 = 8,388,608` bytes,
approximately **8 MiB per state instance**, excluding other storage.
**These caches are not E.** Staleness identity must bind parent E generation,
exact executable model/composition, StateProfile, read site, adapter/B1 weights,
compiler/lowering identity and physical precision/layout. E_g→E_g+1 makes
dependent materializations stale; eager, lazy or scheduled rematerialization
remain target/compiler choices, not a frozen cache policy.


## Realization space and persistent-KV controls

Program N is not a one-dimensional ladder from KV toward cognition. Its
research dimensions are orthogonal; B1 selects one reference point.

| Dimension | Research space |
| --- | --- |
| Representation | Persistent KV baseline, latent slots, associative matrix, recurrent/SSM, low-rank, sparse/routed banks, fast-weight/neural state and future representations. |
| Interaction | Cross-state attention, gated projection, recurrent/state-space interaction, sparse routing and future qualified mechanisms. |
| Update | Read-only, end-of-run, slow block-wise, bounded decode-time, per-layer and external Reconcile. |
| Physical lifecycle | Identity/versioning, paging, sharing/COW, residency, derived caches, checkpoint, transactions, movement and schedule. |

Associative, recurrent, SSM, low-rank, sparse/routed, fast-weight/neural and
hierarchical multi-timescale experiential streams remain alternatives, not
scheduled simultaneous implementations. B1 starts with latent slots,
cross-state read and slow bidirectional update.

Persistent prefix/KV remains a useful physical foundation, training-free
standard-memory baseline, compatibility experiment and ablation/control. It
supports context-derived-state and segmented persistent/local-attention
research; it is not Program N's destination. **Current prefix reuse != E;
persistent KV != proven State Read.** Context-only provenance-bound lowering
remains OPEN despite ordinary context execution being available.

A standard/prefix realization may combine a persistent K/V bank and a local
mutable K/V bank under **one mathematically correct attention normalization**.
Two separately normalized attentions added together are not equivalent.
Unified online-softmax/LSE across segments is a future physical research path,
not B1 dual-stream: E has its own computational trajectory and update semantics.


## Compiler, execution DAG and admitted backend target

Current [typed compiler foundations][compilation] include STATE, READ_STATE,
WRITE_STATE, SSA values, state versions, functions, regions, typed effects and
producer dependencies at their documented implemented scopes. These establish
bounded representational capacity, not B1. The current globally ordered effect
baseline and serial lowering are too conservative for full dual-stream execution.

C's future Semantic Model IR must describe primary tensor/state values,
persistent E values, state-consuming/producing operations and blocks, R→E and
E→R interactions, gating/merge, state normalization, version transitions,
multi-result computation, explicit state roots, independent dependency branches,
bounded update barriers/control structure and state-aware entrypoints.
Trainable parameter roles and augmentation semantics belong here; optimizer
policy is not required to describe a trainable model.

Program/Execution IR must distinguish effects by resource/state root, for example
`READ/WRITE(local_attention_state)`, `READ/WRITE(local_recurrent_state)`,
`READ(E_generation_g)` and `WRITE(E_working_g+1)`. Ordering is required for real
data dependencies, conflicting roots or explicit ordered semantics, not merely
because both operations are stateful.

```text
                      normalized R
                       /        \
                      ▼          ▼
                PrimaryMixer   StateRead(E)
                      │          │
                      └────┬─────┘
                           ▼
                        GateMerge
                           │
                           ▼
                        FFN / MoE
```

Execution IR must preserve producer dependencies, state-root conflicts, effect
ordering, last use, update barriers, merge synchronization and candidate-state
transitions. It must not encode CUDA stream IDs. Target/Schedule lowering may
execute PrimaryMixer and StateRead concurrently or serially before GateMerge
when semantics allow either. Legal independent DAG nodes do not require CUDA
concurrency; target evidence chooses the schedule. No speedup/latency prediction
is adopted.

Admitted CUDA execution is part of the real B1 architecture, not a late optional
optimization. Correctness-first backend primitives include state and cross-state
Q/K/V projections, State Read attention, State Update interaction, gate, merge,
normalization, state-bank addressing, candidate-state write and derived
materialization. Qualify each against independent reference operators before
fusion research such as projection+read, read+gate, gate+residual merge,
update+normalization or projection-cache materialization. Fusion may change
implementation details, never model mathematics.

Refoundation .1 established universal typed computational authority, explicit
state/effects, a common compiler/runtime boundary and family-independent
execution ownership at the currently admitted scope. Its qualification wave
now owns broad replay; B1 added no operators, implementation work or closure
gates to that cutover.


[compilation]: ../architecture/compiler-ir.md

## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
