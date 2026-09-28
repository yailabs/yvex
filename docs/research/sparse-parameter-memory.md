<!-- docs:metadata
title: Sparse Parameter Memory and Phase Liveness
id: yvex.research.sparse-parameter-memory
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Sparse Parameter Memory and Phase Liveness

**Can phase-aware representations reduce real working sets without changing model semantics?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

Can phase-aware representations reduce real working sets without changing model semantics?

Require exact addressing/hash oracles, equivalent phase transitions, truthful physical residency and complete-request measurements. Addressability or allocated capacity cannot stand in for observed movement.


## Conditional sparse parameter memory

This OPEN target is an immutable model-parameter domain whose logical table can
be enormous while execution dynamically selects only a small subset of rows
from input/state-derived addresses. Engram in [A11](architecture-spectrum.md#a11-deepseek-v41-flash-target-pressure)
is its first selected Spectrum pressure, not a new generic runtime or Program N
realization. Selection as a research pressure does not schedule implementation.

Three n-gram concepts have independent meaning and owners:

| Concept | Computational path | Ownership |
| --- | --- | --- |
| Lexical n-gram addressing | Exact tokenizer → token IDs → ordered windows → qualified key/hash/address policy | Source facts under R; model/compiler semantics under C |
| N-gram-addressed parameter memory | Key → indexed immutable table read → selected rows → model projection/interaction/gate | C semantics, P representation, S resources, backend execution |
| N-gram generation proposal | Committed token history → suffix/n-gram heuristic → proposed tokens → generic speculative verification | G; independent of model-internal conditional memory |

Future addressing identity binds the exact tokenizer/source revision, token-ID
domain, compressed-token mapping if present, n-gram sizes, history padding and
boundary rules, hash/address algorithm and version, cardinality, head/table
partitioning, collision/search semantics, masking and image/non-text participation.
Source projection/import seals those facts; runtime does not reconstruct them
from strings. Changing the tokenizer can invalidate table compatibility even
when backbone geometry matches. This target freezes no public n-gram ABI.

Semantic Model IR must eventually express deterministic indexed parameter reads,
token/window-derived keys, sparse row populations, row combination/projection,
gating/residual interaction, explicit table identity and tokenizer-derived
dependencies. It must not treat a huge table as one dense execution operand
when only selected rows are needed. Unknown or malformed addressing semantics
fail closed. These are semantic requirements, not frozen operation names or an
`Engram` universal primitive.

The physical model/state classes remain separate:

| Class | Selection and lifetime meaning |
| --- | --- |
| Dense immutable parameters | Ordinary model computation; often resident |
| Routed MoE experts | Immutable parameters selected by learned routing |
| Conditional sparse parameter memory | Immutable rows selected by token/feature-derived addressing |
| Local mutable sequence state | Attention KV, recurrent, convolution and decoder state |
| Experiential Computational State E | Future mutable/versioned, cross-request, StateProfile-bound computational experience |
| Latent Deliberation State L | Unfinished computation under its execution compatibility contract |

These classes may share mechanisms, not semantic ownership or lifecycle by
definition. There is no universal `memory` object. Engram is learned parameter
memory, part of exact weights, and unchanged by ordinary inference. It is not
application retrieval. E instead has future working-state-derived realization,
Lower/Reconcile and model State Update; L preserves unfinished work.
**Engram != E; Engram != L; Engram lookup != Recall or YAI retrieval.**
Engram n-gram parameter lookup != State Read; an Engram row cache != E residency;
replacing an Engram table or hash policy != model State Update. N does not own Engram, and
Qwen-first B1 research/post-training remains independent.

P's future table representation includes row dtype/qtype and block geometry,
scales, packing/alignment, file-backed or mmap-compatible layout, partitioning,
device gather representation, cacheable rows and admitted backend compatibility.
This is broader than ordinary tensor quantization. Logical table size does not
require full residency or the same policy as dense weights.

A possible physical hierarchy is complete immutable backing → file/NVMe/mapping
→ OS page cache or bounded host residency → YVEX hot-row/hot-page cache → optional
device row cache → indexed backend execution. This does not require every level,
SSD I/O on every lookup, or reliance on the OS cache alone. Actual policy needs
measurement; source/artifact integrity still governs all reopened backing.

Future resource evidence distinguishes logical table bytes, mapped/backing bytes,
resident host/device bytes, cache capacity, cached row/page populations, unique
rows requested, logical row bytes requested, physical bytes read/moved, hits,
misses, evictions, prefetches and current/peak residency. Mapped, prepared,
allocated, addressable, resident, moved and observed are different facts, not one
"memory used" value. Model mapping, dense prepared weights, routed experts,
conditional tables, state backing, workspace and derived caches need separate
accounting even where storage mechanisms are shared.

Future CPU/CUDA execution may require key/hash preparation, batched indexed
gather, quantized row decode, head/column combination, projection, gating and
residual addition. Independently testable reference operations and admitted
backend numerics precede any gather/dequant/combine/project/gate fusion; fusion
cannot change source mathematics. No such kernel is implemented by this target.


## Phase liveness and quantized runtime state

Compiler-visible executable-resource lifetime should eventually influence
physical residency. C proves program/component dependencies and liveness for
immutable parameter groups, derived state and workspaces; P specializes legal
representations and residency opportunities; S applies budgets, movement and
cache policy. Retain, prefetch, stream, evict and rematerialize decisions derive
from compiled truth and measured hardware facts, never a family-name heuristic.
Current engine accounting and bounded dependencies do not establish this target.

CED exposes encoder/prefill parameters, hot experts, Engram accesses and workspace,
then an explicit state/result boundary, then decoder/generation parameters, hot
experts, projected global state/KV, local/SWA state and workspace. The phases have
different computation, state production/consumption and resource requirements.
Resources needed again in a later phase must remain available or be restored
before reuse; encoder/decoder membership alone is not an eviction proof.
This is stronger than merely recognizing a T5-like encoder-decoder or applying
model-wide SSD streaming. No equal byte populations, exact half-model eviction,
fit, speedup or throughput follows from the layer split. Program/Execution IR
preserves dependencies and legal independence; Target/Schedule chooses physical
execution and synchronization, without mandatory CUDA concurrency.

Quantized runtime state is a separate target from weight quantization. Semantic
state geometry need not share activation precision; P must eventually bind
state dtype/qtype, packing/scales/layout and backend representation independently.
V4.1 FP4 KV pressures that contract; future long-context, recurrent/SSM and B1 E
representations may also use it. This makes no FP4 choice for B1 and qualifies
neither low-bit state nor a new E implementation.

For these targets R/Source owns immutable source, tokenizer/addressing facts,
table identity and checkpoint/family relations. C owns CED programs, shared
cross-layer state, addressing/indexed reads, source interaction/mHC semantics
and phase liveness. P owns table/state representation and backend compatibility.
S owns residency, row/page movement, expert/conditional caches, phase-transition
resources, state lifetime and accounting; S does not own n-gram meaning. G owns
only the separate generation proposals. Q requires official/reference conformance,
address/hash vectors, positive/refusal cases, cache-policy numerical invariance,
phase-residency equivalence, low-bit state numerics and the complete terminal
vertical before broader behavior/performance claims. Backend owners execute
admitted primitives. N remains the independent B1/E owner.


## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
