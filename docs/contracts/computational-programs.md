<!-- docs:metadata
title: Computational Program Contract
id: yvex.contracts.computational-programs
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Computational Program Contract

**Exact types, operations, effects, passes and immutable program identity.**

[Up](README.md)

## Representation and ownership

The token-forward runner consumes a compiler-derived interface from physical
operands and results, not hidden-width/vocabulary/layer fields in the historical
decoder. Multiple token embeddings intersect their admitted vocabulary bounds;
embedding width need not equal returned hidden width. Each state input must have
exactly one produced successor in the result signature. This bounded, linear-time
projection is shared by cold compatibility admission, runner preparation,
generation admission and output-head preparation. It is not another persisted
model representation. The engine view no longer exposes a decoder plan. The
output binding retains the decoder producer identity for existing report/input
lineage only; model context comes from the authenticated binding envelope,
distinct from program row population and runtime resource admission.

Startup admission does not require an allocated session. The state provider's
`yvex_sequence_state_plan_measure` validates the program-derived bindings and
measures its committed/candidate F32 banks before allocation. The same geometry
authority drives provider layout; generation does not reconstruct recurrent
sizes from decoder layers or assume that allocation has already happened.
This is physical storage admission, not Native Cognitive State support.

A module owns dimensions, interned logical types, functions, blocks, operations
and uniquely defined values. Construction copies requests; sealing verifies and
freezes the module. IDs are module-local handles. Construction may relocate a
view, whereas sealed views last until module close. Neither kind of handle is
a runtime device-result publication generation, engine lease or checkpoint.

| Computational form | Implemented meaning | Persistence / consumers |
| --- | --- | --- |
| Imported program | Family/source interpretation as typed operations, parameter references and explicit state dependencies | Qwen and MiniMax program projection plus exact-source Mamba2 pure-SSM forward/output retained through artifact and binding |
| Canonical program | Same module infrastructure after verified alias and dead-pure-value passes | Immutable compiler object, canonical diagnostic text and semantic identity |
| Straight-line execution form | Entry-local value slots, producer dependencies, serial effect order and last-use boundaries | Qwen and MiniMax binding compilation consume this verified lowering; general executable-region breadth is not claimed |
| IR binary v1 | Explicit-field encoding reopened through constructors, static dialect resolution and the verifier | Internal serialization contract tested by roundtrip/truncation; not embedded in current v17 runtime bindings |
| Existing Transformation IR | Parameter derivation, ordered source contributions and provenance | Existing source-to-package authority; not replaced by program operations |
| Parameter physical projection | Source-bound program constants joined to transformation terminals and physical package decisions | Compiler-owned terminal handles and a distinct identity; no payload access or target schedule |
| Tensor program v1 | Verified pure rank-2 BF16 instructions, operand/result slots and admissible row populations | Retained bounded operator consumer and model-plan v5 import; not independently serialized in native v7 |
| Physical program v1 | Typed token/tensor/state slots, exact parameter handles, admitted implementation contracts and serial dependencies | Native model-plan v8 forward/output/component consumers; older decoder/output compatibility normalizes once after binding authentication |
| Operator schedule v1 | Canonical operation nodes, data/order/state edges, target/draft populations and semantic lineage | Model-plan v8 persists the exact schedule; DeepSeek runtime resolves layer work from it while separately authenticated implementation plans supply admitted numerical contracts |
| PEIR v5 package projection | Authenticated terminal roles, qtypes, encoded ranges, stable layout and sharing | Persisted in runtime binding; contains no deployment specialization or backend launch choice |
| Physical program / target specialization | Parameter-bound computational work and admitted implementation substitutions | Physical-program identity remains distinct from PEIR, deployment specialization and backend-local launch mechanics |

Semantic tensors contain scalar type and logical shape, not GGUF qtypes, CUDA
layouts or alignment. Types include scalars, tensors, semantic-domain state,
tuples and program signatures. Program signature types may describe state-bearing
argument/result lists. Actual SSA values cannot pack state into an aggregate:
state remains a direct, independently verified input/result until an aggregate
ownership contract exists. A program handle describes a signature, not captured
mutable state.

Shapes use static extents or shared named dimension symbols with positive
minimum/maximum/multiple constraints. Verification rejects undefined symbols,
simultaneously static/symbolic extents and overflowing element bounds. Neural
operation verifiers check contraction and preserved dimensions. This is not a
general shape-expression solver or qualified dynamic-shape runtime.


## Operations, regions and effects

Operation definitions are selected by qualified name **and semantic version**.
An omitted construction version selects v1 for compatibility, never the latest
definition. Wire import requires the exact nonzero version; passes preserve it.
Distinct versions may coexist, while duplicate name/version pairs refuse.
`sequence.gated_delta` v1 retains BF16 parameter operands. V2 retains BF16
convolution/time-bias operands and F32 decay-log/normalization operands, with
F32 recurrent/convolution state in both versions. Physical
`gated_delta.mixed.f32state.v2` joins exactly those parameter classes; neither
lowering nor execution changes an F32 source vector to BF16.

Output-head plan v3 (schema 5) permits a tied `TOKEN_EMBEDDING` parameter with
`separate_output_head=0`. V1/v2 retain their separate `OUTPUT_HEAD` contract.
The compiled output program must reference the same exact physical tensor as
the forward embedding operation; equal dimensions are insufficient. Residency
owns that parameter once. The normal output/logits lifecycle remains common.

Static dialect tables bind a qualified operation name and semantic version to
arity, attribute schema, effects, region count and a typed verifier. An imported
program cannot redefine an operation or supply code pointers. Unknown operations,
unknown/duplicate attributes, non-finite numerical attributes and invalid types
fail before seal. Logical parameter references carry an exact source identity and
symbol; payload bytes remain outside the program.

`core.call` resolves a function symbol and checks its complete signature.
`core.loop` carries explicit values through an index-controlled region;
`core.if` has two typed regions. Return/yield terminators are verified against
their owner. Implicit call recursion is refused. Neural definitions include
embedding, linear contraction, normalization, activations and elementwise
operations; registration and verification do not establish backend execution.

Effects distinguish state read/write, RNG, publication and ordered computation.
Functions declare an effect envelope and calls inherit the callee's envelope.
Operations cannot read values that do not dominate them. A consumed state
version cannot be read, updated or returned again; regions receive state as
explicit block arguments instead of capturing an outer mutable owner. State
versions describe semantic dependencies, **not physical copies**. No KV, paging,
residency or Native Cognitive State implementation is introduced by this IR.

The compiler lowers canonical straight-line entries through
[`program_execution.c`](../../src/graph/program_execution.c). Parameters remain
symbolic; operands/results become compact entry-local slots. Each work item
names its producer dependencies, and each value records its last use within
the entry. Returned values still require the separate consumer/publication
lifetime; last use is not permission to retire a published result.

The initial lowering preserves one serial chain of declared effects, including
state reads before subsequent writes and effect completion before return. It
does not claim parallel state scheduling or infer that a new semantic state
version needs a new allocation. Pure data dependencies are separately explicit.
Calls must first be legalized; unlowered regions fail closed rather than being
silently flattened into a DAG. Region representation in Semantic IR therefore
does not imply an executable region backend.

The execution identity/dump uses sorted entry symbols and entry-local slots,
bound to the semantic identity. Construction-local module IDs are inspection
links, not serialized identity. This form still has no backend selection,
placement, physical activation layout or production binding serialization.


## Passes, identity and refusal

| Ordered pass | Transformation | Verification / evidence |
| --- | --- | --- |
| `legalize.direct_calls.v1` | Expand acyclic direct calls into explicit operand/result and state dependencies | Repeated stateful calls retain successor versions; expansion/nesting budgets fail closed; Qwen uses this for 64 FFN calls |
| `canonical.value_aliases.v1` | Replace `core.identity` results with their input values | Typed input/output verification; no state alias escape |
| `canonical.dead_pure_values.v1` | Remove unreachable pure computations | Preserve effects, terminators, state transfers, region parents and dependencies |

Each pass reads an immutable module and publishes a distinct verified module.
Failure publishes neither a partial module nor partial pipeline receipts. The
receipt binds pass name/version, input/output identities and operation counts.
The tested canonical pipeline is deterministic and idempotent. These are real
rewrites, not physical/backend legalization passes; the latter remain migration
work.

The canonical text sorts function symbols and attributes and renumbers SSA
values by traversal. Scalar floating attributes use exact hexadecimal IEEE bits,
avoiding locale-dependent decimal formatting. Semantic identity hashes this
field-defined projection with source lineage, never C memory, padding, pointers,
paths or timestamps. Removing a representational alias changes the imported-form
identity; equivalent alias-free forms converge after canonicalization. Physical
model bytes are not part of this logical program encoding.

Only sealed modules may be retained by another compiler owner. The module owns
its storage once; an atomic lifetime reference keeps immutable views alive after
the source projector closes. Retention is not another semantic identity, a
session-state copy or a transient device-publication generation.

The Qwen text projector emits `forward` and `output` entrypoints and a reusable
`dense_ffn` computational function with explicit input/weight arguments. The
ordered pass pipeline expands its 64 calls before execution-record projection;
the function remains inspectable, not a new runtime architecture. BF16 logical
publications remain distinct from F32 recurrent/convolution state and F32 logits.
Native model-plan construction requires the exact sealed semantic/execution
lineage and lowers the complete `forward` program directly. It does not compile
and then discard a separate physical `dense_ffn` product, nor manufacture one
from decoder metadata when the source program is absent. Historical v3/v4/v5
containers retain their validated FFN translation at binary import only; that
compatibility path is not an alternative native compiler authority.
Linear projection, normalization, residual addition and the SiLU product are
explicit operations. The SiLU product rounds the activation to its logical type
before multiplication and rounds the product again; physical fusion must retain
both rounding points. Stateful attention and gated-delta
operations consume projected values, exact parameter references and state
versions; they do not allocate state or encode a family/layer enum in IR core.
The neural BF16 linear contract accumulates in F32 and rounds once to its declared
result type; a BF16-to-F32 output head does not insert a BF16 publication.
The gated attention contract retains interleaved query/gate splitting, one-plus
Q/K RMSNorm, half-split partial RoPE and BF16/F32/RNE causal attention followed by
sigmoid gating. The gated-delta contract retains F32 recurrence and one BF16
output publication. These describe current internal numerical classes, not
upstream conformance.

Qwen's current attention/decoder compilation records are now projected from that
verified program, which the compiler retains and binds into its identity. They
remain compatibility/provider/report views, not the token-forward operation
sequence. [`decoder.c`](../../src/runtime/decoder.c) is a runner over physical
SSA instructions; the procedural layer loop, layer-indexed weight directory,
per-layer linear preparation and separate FFN invocation have been removed.
Generation iteration and the output/logits consumer remain above this forward
program and consume its derived interface, not a historical decoder plan.
The exact current Qwen artifact has completed bounded CUDA generation through
this path: 15 prompt tokens, observed tokens 3793 and terminal 248046, one
nonterminal commit and final sequence position 16, without prefix reuse.
This is real execution evidence, not independent whole-model preservation or
upstream conformance; those qualification obligations remain separate.
The Qwen source fixture proves 48 recurrent operations,
16 attention operations, 112 distinct state inputs and all 851 text parameters;
physical token-forward lowering produces 1,732 instructions with 14 reusable
tensor storage slots. The separate output entry lowers to two instructions with
BF16 logical input and F32 logits, sharing forward semantic/execution identity.
This fixture does not execute the full model.

[`program_physical.c`](../../src/graph/program_physical.c) lowers explicit
operands/results, logical publications, parameter tensor IDs, state roots and
last-use intervals. It admits embedding, BF16 linear/RMSNorm/SiLU/add, gated
delta, gated causal attention and the two-result mHC head. A state successor is a semantic dependency,
not an allocation or a physical copy. The current provider specialization
admits one transition per state root per invocation; unsupported state flow is
refused, not silently linearized. Exact package identity, qtype, row geometry
and encoded extent are checked against PEIR at cold binding admission.

[`program_device.c`](../../src/runtime/program_device.c) binds static
implementation contracts once and executes slots without source/role/layer
lookup. [`program_kernels.c`](../../src/runtime/program_kernels.c) owns exact
parameter handles and reusable linear specializations, including preparation
budgets. [`program_sequence.c`](../../src/runtime/program_sequence.c) stages
individual recurrent/attention operations through existing providers; it does
not own layer composition or transaction commit. Recurrent provider bindings
derive from IR state-root slots rather than decoder ordinals. Attention still
uses a cold-validated derived provider plan. Operation scratch protects SSA
inputs from in-place Q/K normalization and RoPE.

The bounded CUDA fixtures execute complete imported forward programs with
nonzero BF16 weights through real recurrent, attention and residency providers:

| Composition | Numerical operations | State inputs/successors | Compared outputs | Compared committed state values |
| --- | ---: | ---: | ---: | ---: |
| Two recurrent blocks | 30 | 4 | 96 | 2,432 |
| Two recurrent blocks + one attention block | 43 | 5 | 96 | 2,624 |

Chunked versus single-token invocation is exact at tolerance zero for both
outputs and state. Cancellation after a transition aborts all participating
state owners without changing committed bytes; retry commits and cleanup
returns allocation to baseline. These are internal cross-path and lifetime
proofs, not an independent model oracle, full-scale Qwen execution or upstream
conformance. The CPU storage/dispatch fixture additionally refuses publication
when cancellation is observed during the final operation, even after all
values were computed. It proves VM contracts, not production CPU model kernels.
