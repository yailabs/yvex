# Compilation and Artifact Architecture

Status: current implemented architecture

This document owns the explanation of how YVEX turns an identified source
snapshot into an admitted physical artifact and runtime binding. Normative
artifact requirements are in the [Artifact and Admission Contract](../contracts/artifacts.md).

## Pipeline

![Family interpretation projects coordinated computation and parameter-package compilation lanes. Exact parameter lineage and PEIR package terminals join the physical computational program before runtime binding and deployment specialization.](../diagrams/physical_compilation.svg)

*Figure 2 — Coordinated compilation lanes and source-to-engine promotion.
Computation meaning and parameter/package derivation remain distinct until the
identity-preserving parameter join. Package truth, runtime binding, deployment
specialization and engine resources are also distinct identities/lifetimes.
Missing semantics or resources refuse at their owner, never imply the next
stage.*
[Editable source](../diagrams/physical_compilation.json).

## Source intake and trust

Source intake records repository/revision, configuration, tokenizer sidecars,
shards, tensor names, shapes, dtypes, and byte ranges. Structural inventory and
payload trust are separate. A source can be inventoried without having every
payload digest authenticated.

The retained source snapshot is immutable and indexed. Downstream owners
consume typed facts and exact bounded ranges; they do not rescan source headers
or infer semantics from filenames.

Payload admission has an explicit bootstrap boundary. `source verify`
promotes verified metadata/header provenance to source-manifest v3 only after
reading every shard and matching its authoritative provider SHA-256. The v3
manifest lives outside the source snapshot, binds the ordered aggregate payload
identity, and can be reopened without a second full payload pass. Transformation
planning still consumes zero payload bytes; execution admits ranges only from
that trusted identity.

## Logical projection and transformation

Family owners interpret exact source facts into model topology and canonical
roles. The transformation plan then binds every terminal output tensor to its
ordered source contributions and typed operations. Plan construction is
artifact-neutral and payload-free.

Current compilation has two coordinated concerns. The **computation lane**
projects family semantics into a sealed Semantic Model IR, may retain a native
typed `yvex_ir_module`, lowers legal entrypoints to `yvex_program_execution`,
then joins exact package parameters before selecting admitted implementations
in `yvex_program_physical`. The **parameter/package lane** projects verified
source through Transformation IR,
transform binding, artifact lowering, representation/quant decisions, writer,
admission and artifact materialization into PEIR package terminal truth. The
lanes join when each symbolic computational parameter resolves through its
Transformation IR lineage to one exact PEIR/package realization. They do not
form an undifferentiated IR-to-GGUF pipeline, and family coverage is not
identical.

Each family projects immutable source and terminal recipes through the bounded
compiler sink. The generic compilation owner alone allocates mutable builder
state, assigns canonical value and node ordinals, validates expected source and
terminal populations, seals the IR, and releases failed construction. Family
code cannot manipulate or persist the mutable builder representation.

The family projection seals source-authored context, MoE, output, DSpark and
persistent-state geometry into one pointer-free model-execution descriptor. It
also projects every main and draft attention layer, together with the numeric
contract required to interpret it, into compiler-owned Semantic Model IR
storage. Common planning consumes those identity-bound facts instead of
branching on a target name, retaining process-local family payload, or
repeating family constants. Synthetic descriptor tests vary the principal
dimensions and mutate the source projection after sealing to prove that the
common path remains model-derived and immutable.

`yvex_semantic_model_ir` and `yvex_ir_module` are not synonyms. The former is
the sealed model-semantic aggregate: it carries family adapter identity,
execution descriptor, numeric contract, attention/decoder/composite facts,
references and semantic identities. The latter is the native typed
computational-language object with dimensions, types, functions, blocks,
operations, values, effects and explicit state; a Semantic Model IR may retain
one as its `program`. Qwen and MiniMax currently use native typed programs for
their admitted computational paths, DeepSeek retains a canonical operator
schedule alongside sealed semantic and derived implementation records, and
Mamba2 establishes representability without an admitted executable target.

The compiler-facing family adapter supplies one bounded graph compiler and the
family's operator-composition callback. Family projectors are consumed only
while sealing Semantic Model IR. Generic graph lowering reads the sealed
attention topology and converts it into derived attention, MoE and transformer
implementation-plan records;
generic graph code does not enumerate a process-global family registry or
choose a transformer-shaped composition. Family projection callbacks are
absent from runtime model-open and execution.

Transformation execution reads only the ranges named by the sealed plan. It
may select, concatenate, permute, aggregate, scale, convert, or quantize as
declared. It may not rediscover axes, expert ordering, companion scales, or
qtype policy from source names.

## Physical policy

A physical variant resolves physical class, storage qtype, tensor and row
geometry, encoded size, approximation/calibration obligations, policy identity,
and backend-compute availability for every terminal tensor. The requested
backend currently filters candidate feasibility; it does not turn the variant
into a device-specific deployment optimization. Quantization codecs and qtype
geometry are canonical owners, while selection of a qtype for one tensor is a
parameter-representation decision.

Writer and runtime owners consume the resolved variant. They do not pick a
different representation for convenience.

The physical variant owns canonical encoded representation. Physical Execution
IR schema v5 is the package projection for each terminal tensor: canonical role,
scope, coordinates, qtype, row geometry, encoded range, alignment, consumer,
stable layout, sharing, and terminal identity. It deliberately contains no
backend, device, activation, kernel-family, width-crossover, evidence, fallback,
or live-resource policy.

At model-engine open, the runtime combines these authenticated package decisions
with one real backend/device and seals an engine specialization. That
specialization owns the admitted implementation class, activation
representation, legal real widths, fallback-equivalence class, and any
hardware crossover. Actual compatible rows and routed populations still belong
to executable batches and expert worklists. CUDA may select an equivalent
microkernel inside the admitted class, but it cannot infer semantic
compatibility, manufacture width, or select a numerically different class.

Package identities therefore change with model/storage meaning. Specialization
identity changes with deployment-significant implementation facts. Equivalent
warp, tile, grid, stream, and launch geometry stays backend-local and does not
rebuild the package or binding.

## Artifact emission and admission

The GGUF writer plans exact metadata, tensor directory order, alignment,
padding, ranges, and payload bytes before transactional publication. Native
reader and global-layout owners independently validate the emitted container.

Complete-artifact admission additionally binds model metadata, tokenizer
facts, every required tensor role, qtype support, source/derivation/variant
identities, and exact file identity. Structural GGUF validity is necessary but
not sufficient.

Artifact materialization builds and commits an authenticated package mapping:
checked tensor bindings and bounded access to file-backed package ranges used by
the runtime descriptor, PEIR construction and runtime binding. It does not
import a concrete model family, choose a deployment implementation, infer
consumers from tensor names, execute a graph, or establish model support.

Backend/model weight materialization is a separate later mechanism. It turns
admitted package bindings into host, CUDA-addressable-host, device, staged, or
derived executable resources for an engine deployment. Its resources do not
become artifact materialization records or PEIR facts. A derived representation
becomes package truth only if it is deliberately emitted and admitted as a
separately authenticated asset.

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

Runtime binding v16 persists the canonical operator graph identity, Physical
Execution IR v5 package records, and pointer-free compiled tokenizer,
conversation, and model/operator plans. Source-owned syntax and exact tokenizer
component identities enter through the family compiler adapter; tokenizer,
runtime, and server consume the authenticated record without enumerating a
concrete family.

The v16 reader also authenticates accepted v14/v15 bindings. It imports a v14
physical record only when the legacy record names canonical package storage and
does not require its retired derived-layout/runtime-policy fields; the importer
then normalizes that package truth to PEIR v5 before engine specialization.
Unsupported legacy derived assets fail closed. Bindings v7 through v13 remain
explicit rebuild boundaries because they predate the canonical operator graph.
Old bytes are never reinterpreted as v16.

The non-persisted runtime execution profile binds an exact engine generation
and specialization to workload, kernel bundle, generation mode, evidence class,
and typed operation resolutions. It is built inside the opened engine/session
lifetime. It is not a second permanent execution plan.

Artifact drift, binding drift, unsupported qtypes, missing roles, resource
overflow, or incompatible runtime requirements refuse before model execution.

## Executable composition oracle

The CPU-only tiny vertical is the fast composition oracle for this pipeline. Its focused test
owner deterministically generates an untracked GGUF, admits it through the production artifact
contract, compiles the semantic model, operator graph, Physical Execution IR and runtime binding,
then launches the real foreground host with zero engines, loads the fixture,
serves it, unloads it without stopping the host, reloads it as a new generation,
and admits two fitting engines concurrently. The production `host status`,
`engine list`, `engine load`, native generation, `engine unload`, `host logs`, and
`host stop` paths must return the expected context, text, identities, typed
completion event, routing refusals, and clean lifecycle. A second build must
reproduce artifact and binding identities, while a corrupted artifact must
refuse without terminating the host.

The fixture adds no production model family and does not establish support, quality, CUDA or
performance for a real model. Its generated artifact and binding remain temporary build evidence,
never repository authority.

## Current family lowering

Exact source inventories, role counts, and physical-policy limitations belong
to the [family records](../model-families/integration.md#current-family-boundaries).
Shared compilation preserves source derivation and shared operands instead of
duplicating bytes for multiple plans.

The typed architecture is not yet a complete decoder for every sequence
mixer. Mamba2 has source and recurrent-component evidence but no admitted
SSM-only artifact/decoder binding. Mandatory FFN or rotary/KV assumptions may
not be satisfied with fictitious roles to manufacture READY.

## Typed computational programs

The native program-language foundation is owned by
[`include/yvex/internal/ir.h`](../../include/yvex/internal/ir.h) and
[`src/ir/module.c`](../../src/ir/module.c). The current admitted serving paths
now have one compiler-owned computational lineage. Qwen token-forward and
output execution consume physical SSA. DeepSeek target/draft execution consumes
the canonical operator schedule retained by model-plan v8 while its attention,
MoE and transformer plans remain derived physical implementation records.
MiniMax neural components consume typed physical programs; media and product I/O
remain outside neural IR. Attention/state providers and runner/result records
remain derived lifecycle views and do not reconstruct topology. Refoundation
.1 completed this current-consumer authority cutover; `.1.QUALIFICATION.0` owns
the broader replay and independent evidence campaign.

### Current consumer cutover

| Consumer | Compiler/runtime authority after refoundation .1 | Remaining evidence or later breadth |
| --- | --- | --- |
| Qwen 3.5 | Forward/output entries share compiler lineage; the slot runner executes physical SSA and legacy decoder bytes normalize only at cold admission. Attention providers and reports are one-way derived views. | Whole-model before/after preservation and authoritative upstream conformance. |
| DeepSeek V4 / DSpark | Canonical operator graph owns embedding, heterogeneous attention/MoE pairs, target/draft data and state dependencies, final/output and draft projections. Binding v16/model-plan v8 retains it; runtime schedules from it and uses separately authenticated physical implementation plans. | Official/reference conformance, the known whole-model CPU/CUDA discrepancy and performance qualification; no second family runtime remains. |
| MiniMax H3 | Text, multimodal text, vision, visual dense/prefix, audio signal and joint prepare/step/forward compile to physical SSA; common component owners execute admitted work; old procedural neural executors are removed. | Asset-dependent trajectory/full-scale qualification; bounded component preservation is not full-model or upstream conformance. |
| Mamba2 | Pure SSM representable without attention/KV | Preserve representability only here; A01 executable repair remains queued and PARTIAL |

The ownership cutover is explicit per architecture consumer:

| Family | Source identity | Architecture importer | Semantic / execution owner | Physical / target owner | Runtime consumer | Historical computational authority | Qualification depth |
| --- | --- | --- | --- | --- | --- | --- | --- |
| DeepSeek V4 / DSpark | Source/catalog revision, selector, representation and target/draft relation | `model/families/deepseek_v4.c` interprets source schema; `graph/families/deepseek_v4.c` projects canonical semantics | Semantic Model IR plus retained operator graph own target/draft topology, dataflow, state and ordering | Transformation IR, PEIR and typed attention/MoE/transformer programs and plans | Generic transformer/generation owners resolve the retained graph and invoke admitted work | Family containers retain import and irreducible operation policy only; no warm family topology owner | Real target/draft, generation, CPU/CUDA and OpenAI regressions; official vectors not executed; whole-model backend gap remains OPEN |
| Qwen 3.5 | Source/catalog revision, selector and representation | Qwen model/graph importers project source topology | Native typed forward/output module and physical SSA own current text computation | Parameter projection, physical program and exact recurrent/attention state bindings | Slot-based program runner and generic state providers | Legacy decoder schemas are cold-import compatibility only; procedural forward loop removed | Recurrent/hybrid numerical lanes and bounded exact-artifact generation; upstream conformance not executed |
| MiniMax H3 | Source/catalog component and representation identities | MiniMax model/graph importers project neural component interfaces | Typed text, vision, audio and joint component programs | Physical component programs and admitted backend operations | Common component executor and runtime lifecycle | Family code retains import and irreducible fused-operation meaning; old neural procedural loops removed | Bounded component preservation; full asset-dependent trajectory and upstream evidence remain open |
| Mamba2 | Exact source/catalog snapshot | Mamba2 source/graph importer | Pure-SSM topology is representable without fictitious attention, KV, RoPE or dense FFN | No claimed executable artifact/target lowering | None claimed by refoundation .1 | No Transformer-shaped dependency introduced | Representation/source scope only; A01 remains PARTIAL and blocked behind qualification |

### Cutover acceptance boundary

The [signal compiler](../../src/graph/signal_program.c) projects source-declared
channel-first convolutions, normalized/transposed convolutions, alias-free
Snake activations, residual branches, ordered means and output clamping into
typed operations. Static channel/sample geometry and a bounded batch symbol
determine operand/result storage. Source parameter names are resolved at cold
binding only; the backend receives admitted operations and encoded weights,
not decoder stages or tensor-name templates. One bounded scratch allocation is
reused across signal operations. The prior CPU and CUDA complete alias-decoder
interfaces and procedural loops are removed.

The current audio recipe projects seven upsampling stages and 914 parameters.
An exact-artifact, one-frame before/after replay preserves all 800 F32 samples
bitwise on each backend. This establishes bounded migration preservation, not
upstream conformance, arbitrary-duration audio qualification or a speedup.

The following is the implemented ownership pipeline at the currently admitted
scope. Individual model architectures and operations still require their own
qualification; the pipeline does not claim universal family support.

```text
verified source -> family interpretation
                        |                 |
                        v                 v
             sealed Semantic Model IR    Transformation IR
                        |                 -> transform binding / artifact lowering
             retained native typed       -> representation plan / writer
             program when present        -> artifact admission / materialization
                        |                 -> PEIR package terminal truth
             execution lowering                    |
                        +------------+--------------+
                                     v
                      exact program-parameter join
                                     |
                      physical computational program
                                     |
                          compiled model plan
                                     |
                            runtime binding
                                     |
                 deployment specialization / engine
                                     |
                      state providers + backends
                                     |
                       typed results + evidence
```

| Boundary | Required unique authority | Current cutover evidence |
| --- | --- | --- |
| Verified source / import | Source identity, configuration, tokenizer, parameter roles and component relationships | Mamba2 source inspection and Qwen text compilation project typed programs |
| Semantic Model IR | Sealed family/model aggregate, semantic identities, numeric obligations and optional retained native program | Qwen and MiniMax retain typed programs; DeepSeek retains semantic topology plus its operator schedule; pure-SSM representability is compiler-owned. |
| Native typed program | Functions/blocks, operations/values/types, shapes/attributes/effects and explicit state dependencies, without payload or backend ownership | Current Qwen and MiniMax computational paths use `yvex_ir_module`; not every family exposes the same native-program breadth. |
| Program Execution IR | Legalized entrypoints, compact value slots, producer dependencies, serial effect ordering and last-use boundaries while parameters remain symbolic | Qwen and MiniMax binding compilation consume this verified lowering. Calls require legalization and general executable regions/parallel scheduling remain later breadth. |
| Transformation IR / binding / artifact lowering | Source-to-terminal parameter derivation and identity-bound source ranges, without model computation or payload reads during planning | Exact source constants retain sealed derivation; generic transform legalization remains bounded. |
| Parameter representation plan | Dtype/qtype, row geometry, packing, alignment and package layout decisions | Current quant plan/physical variant is an implemented representation recipe, not a complete automatic Program P result. |
| PEIR package terminal truth | Authenticated terminal roles, identities, qtypes, row geometry, encoded ranges, layout and sharing | Built from admitted artifact materialization and runtime-descriptor facts; no backend/device/activation/kernel/residency decision. |
| Program-parameter join | One identity-preserving package realization for each used computational parameter | Transformation terminal lineage and PEIR decisions resolve symbolic constants before invocation; payload lookup is not deferred to execution. |
| Physical computational program / target choice | Admitted operation implementations, dependencies, exact populations and physical value contracts after exact parameter join | Serial physical SSA owns Qwen/MiniMax program work; DeepSeek runtime resolves scheduled attention/MoE work from the retained canonical graph and separately authenticated implementation plans. |
| Executable binding / runtime | Authenticate immutable execution truth; own engines/runners/sessions/scheduling/lifetimes | Runtime binding v16 and model-plan v8 carry canonical schedules plus physical programs/plans. Historical v3-v7 forms import at the schema boundary; warm execution does not invoke family importers. |
| State providers / backends / evidence | Physical state mechanisms and CPU/CUDA execution publish typed results and observations | Existing owners and producer-owned transient-result lifetimes preserved |

The retained canonical graph, PEIR and physical computational programs answer
different questions: the graph owns executable topology, dependencies and state
flow; PEIR owns authenticated package terminal truth; physical programs and
derived implementation plans own admitted computational work and numerical
contracts. The runtime cross-checks rather than reconstructs these facts. Generation repetition
remains runner policy above model forward computation. Native Cognitive State
remains OPEN: typed computational state does not introduce semantic-state
ingress or YAI authority into this compiler.

### Representation and ownership

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
| Imported program | Family/source interpretation as typed operations, parameter references and explicit state dependencies | Qwen and MiniMax program projection plus Mamba2 source inspection; Mamba2 executable obligations remain unresolved |
| Canonical program | Same module infrastructure after verified alias and dead-pure-value passes | Immutable compiler object, canonical diagnostic text and semantic identity |
| Straight-line execution form | Entry-local value slots, producer dependencies, serial effect order and last-use boundaries | Qwen and MiniMax binding compilation consume this verified lowering; general executable-region breadth is not claimed |
| IR binary v1 | Explicit-field encoding reopened through constructors, static dialect resolution and the verifier | Internal serialization contract tested by roundtrip/truncation; not embedded in current v16 runtime bindings |
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

### Operations, regions and effects

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

### Passes, identity and refusal

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

### Spatial and dense component programs

The spatial encoder in [`signal_program.c`](../../src/graph/signal_program.c)
projects source-declared stages into `signal.conv2d_slice`,
`nn.spatial_group_norm_silu` and residual SSA operations. Convolution consumes
an explicit O/I/T/H/W parameter and a selected temporal plane; stride, padding,
channel contraction and output geometry are compiler-verified. GroupNorm owns
per-sample channel groups, affine parameters, epsilon and SiLU. The admitted
CUDA implementations reuse the existing kernels; CPU spatial implementation
is not claimed and fails closed during preparation.

MiniMax's keyframe encoder now imports 34 convolutions, 25 normalizations and
12 residual additions through this program, replacing its procedural CUDA
encoder loop. Image decoding/resizing and the existing seeded posterior sample
are outside this migrated encoder scope. Exact 96-value artifact-backed
preservation is not independent model conformance.

The keyframe product callback specializes that source recipe before invoking
the consumer. It lends a completed physical program and cold parameter linkage
through the common component invocation. The consumer binds prepared pixels
and posterior outputs; it neither retrieves the encoder recipe nor compiles
its stages. The compiled owner lives through the synchronous invocation and
is released afterward. The remaining multimodal conditioning adapter is still
a separate mixed-owner boundary, not claimed as fully cut over here.

Cold parameter admission distinguishes ordinary singleton-axis views from the
source-order compatibility package's declared
`preserve-leading-three-fold-trailing-v1` policy. For that scalar-only profile,
the compiler checks the leading axes and the exact product of the contiguous
tail before recovering the logical row view. Equal element counts alone never
admit a transpose, another factorization or quantized-block reinterpretation.

[`vision_program.c`](../../src/graph/vision_program.c) imports patch projection,
learned position interpolation, spatial rotary tables, attention blocks and
normal/deepstack mergers. `encode` returns four typed tensors; `inspect` also
returns declared intermediate observations. Inspection does not introduce a
second layer executor. Spatial dimensions specialize during cold component
admission; runtime binds parameters and invokes the resulting program per image.
The compiler entrypoint also owns source-role-to-parameter-ID translation.
The component resource binder walks admitted physical parameter IDs, not layer
or merger recipes; its invocation receives neither the source recipe nor the
role resolver. Missing compiled parameters fail before execution/publication.
The former CUDA vision executor and its backend entrypoint are removed.

[`dense_program.c`](../../src/graph/dense_program.c) imports the admitted F32
visual dense component into explicit RMS normalization, projection, channel
bias, interleaved Q/K/V partition, unweighted grouped normalization, partial
rotary, full attention, scaled residual and SwiGLU operations. Final LayerNorm
and prefix-row output projection are compiler-owned too. The numerical
contracts distinguish a separately rounded F32 projection/bias from a bias
inside an accumulated projection; BF16 vision/text publication remains distinct.
The CUDA dense decoder loop, resident-decoder request and backend execution
entrypoint are removed. CPU visual composition and source-backed neural prefix
preparation consume the same compiler programs; payload streaming remains a
materialization owner.

[`component_program.c`](../../src/runtime/component_program.c) binds admitted
parameter IDs through the component's cold resource boundary and executes typed
tensor signatures. It verifies capacities and output aliasing, stages results,
checks finiteness, binds result identity to program/residency/input/output, and
publishes only after successful cleanup and the final cancellation check. A
failed checked release remains reachable in the session's cleanup slot. It
does not reconstruct blocks or semantic parameter roles.

Static population changes, including merge reshapes and output slices, are
verified before execution. Target dispatch and prepared matrix implementations
use each operation's result population, rather than imposing the entrypoint's
row count on every operation. Parameter physical formats remain separate from
logical tensor types. Physical-program binary v1 identifies each newly admitted
implementation by name; readers lacking it refuse, without reinterpreting an
older numerical contract. No public ABI or protocol version changes here.

`yvex_program_physical_value_layout` is the compiler-owned invocation view of
activation geometry: fixed extents, the admitted population/multiple, element
count and physical byte count. It distinguishes BF16/F32 in F32 storage from
host-U32 index streams; parameter references and state handles are not activation
allocations. Maximum executable storage is checked during physical verification.
The device executor, tensor stages, prepared links and MoE result carriers use
this view rather than independently resolving shapes. Runtime still validates
actual backing, disjoint views and resource budgets. The view adds no persisted
schema, resource reservation or model-context claim.

### Joint preparation and executable composition

[`joint_program.c`](../../src/graph/joint_program.c) imports conditioned joint
computation into three typed functions: `prepare`, `step` and `forward`.
Component calls inline during legalization. Explicit values carry text refinement,
rotary tables, indexed modality partitions, timestep conditioning, attention,
gated residuals and output projections. The model step consumes retained
preparation results through typed operand links; it does not infer them from
family names. Runtime owns retention, resource leases and transactions, not the
neural dependency order. The old complete joint CUDA executor, backend operation
table, dense-slot reconstruction and private execution arena are removed.

Cold component linkage retains the immutable physical program and its resident
component session. It resolves unique compiled parameter IDs to admitted weight
views once, authenticates that mapping with the program/residency identity, and
charges all retained directories against the component host budget. Session
retirement refuses while a binding is live. Invocation accepts this binding,
not a source-name callback; physical operand compatibility remains stage admission's
responsibility. Prepared-resource identity v4 and joint-result identity v2 include
this linkage, preventing reuse across different admitted parameter mappings.
These are internal transient identity domains, not artifact or wire schemas.

The MiniMax iterative adapter compiles and binds only the exact one/two/three-time
signatures required by the admitted sigma schedule before entering the runtime
transaction. Each iteration selects one retained program; it does not compile,
pad timesteps, or resolve parameter names. Distinct time signatures remain distinct
executable identities, not permission to reuse incompatible prepared resources.
The session retains a bounded collection of these exact prepared programs.
Fixed request identity (layout, condition, positions and partitions) is distinct
from preparation/step and parameter-binding identity. Different time signatures
may coexist within one transaction; changing the fixed request is refused.
Each prepared entry has its own resource handles, borrowed lifetime and checked
cleanup. Catalog growth preserves existing handles and accounts for peak metadata
relocation; all retained programs share the session's host/device budgets.
Internal component resource summary v2 reports aggregate retained resources,
program count and metadata bytes, while its selected identity names one program.
Artifact-backed profiles 1/2/3, revisited in reverse order, preserve 768 values
exactly against corresponding full invocations. This removes warm source
interpretation and qualifies multi-profile retention, not an entire denoising
trajectory or upstream conformance. Full trajectory lanes remain independent.

Source recipe v5 is a transient importer contract. Earlier recipe layouts refuse
before geometry access; this is not a new artifact or public protocol schema.
Physical target specialization admits only registered implementations preserving
operation semantics and precision. Semantic/execution/parameter identities remain
unchanged while physical identity changes. Derived provider views are replaced
and released when target verification rebuilds them, not retained as another
state topology.

`core.observe` is an ordered publication effect with a typed tensor operand and
no computational result. Lowering preserves its dependency and last use. An
explicit runtime sink receives a borrowed observation; missing sinks, callback
refusal and cancellation prevent final publication. Inspection does not turn
all intermediate tensors into retained function results. The selected joint
CPU/CUDA numerical contracts remain explicit; exact pre-cutover fingerprints
prove preservation, not independent or complete-model conformance.

### Physical tensor execution and schema import

[`program_tensor.c`](../../src/graph/program_tensor.c) legalizes a bounded pure
tensor subset: BF16 linear with F32 accumulation, the two-rounding-point SiLU
product, and BF16 residual addition. Physical values distinguish encoded BF16
parameters from F32 activation storage carrying BF16 publications. Instructions
name admitted numerical implementations rather than a family or decoder kind.
Semantic identity, execution identity and physical-program identity remain
separate. Unsupported types, effects, operations, shapes or duplicate output
bindings refuse during compilation/import.

Compiled model-plan **v8** persists the physical token-forward and optional output
programs plus the canonical operator schedule inside runtime binding v16, without
a redundant standalone FFN program. Model-plan v7 introduced the physical
forward/output representation and remains an authenticated import format; v8 is
the current native producer. Its version is independent from package PEIR v5
and physical-program binary v1.
Field encoding and semantic/execution/parameter/physical identities are separate;
the complete Semantic IR module is not persisted in the binding.
[`decoder_import.c`](../../src/graph/decoder_import.c) translates authenticated
v3/v4/v5 decoder geometry and PEIR parameter roles once into the current program.
Version 6 already carries forward work. Older output-head records are normalized
by [`output_head.c`](../../src/graph/output_head.c) into physical SSA after binding
authentication and joined against exact PEIR parameter handles and geometry.
This compatibility importer is never invoked per token. The v4/v5 fixtures
retain exact bytes on re-encoding; v3 retains its existing upgrade-to-v4 writer
behavior. Version 6 remains readable and byte-stable. New v7 containers validate
forward/output lineage, parameter storage and retained runner signature together.
Malformed lengths, identities,
state/effect/type constraints or incompatible attention policy fail closed.
Older binaries without v7 support cannot open v7; existing packages need no weight rewrite.

Output projection uses `linear.encoded.f32.v1` through the same slot executor,
including CPU, CUDA, batched and compatible cross-session consumers. The logits
owner retains sampling/publication lifetimes, not an independent dot-product
loop. Logical activation precision and encoded parameter qtype are distinct:
BF16 weights cannot authorize rounding a declared F32 input. Backend encoded
linear calls declare F32, Q8 or BF16 input policy explicitly. A sparse independent
oracle preserves `1.001953125` exactly with BF16 weights and F32 input; implicit
BF16 packing previously returned `1.0`. This precision correction changes CUDA
output values and is not bitwise preservation of the former output-head path.
It does not resolve the separate whole-model CPU/CUDA numerical discrepancy or
establish upstream model conformance. Legacy numerical recipes outside this
output cutover retain their explicit existing input policy.

The final hyperconnection head is `mhc.head_norm`: one F32 logical input
`[rows, streams, width]`, four explicit parameter values (function, bias, scale,
normalization), positive normalization/gating epsilons, and two BF16 logical
results `[rows, width]`. Result zero is normalized hidden; result one is the
pre-normalized hidden required by draft consumers. Sigmoid stream collapse
rounds to BF16 before RMSNorm; normalization rounds again. Both dependencies
are explicit values, not a hidden borrow from the numerical implementation.
The verifier rejects inconsistent stream/channel geometry, scalar types and
attributes. Physical lowering selects `mhc.head_norm.bf16.v1` and authenticates
all four parameter handles against package PEIR; it does not assume their
encoded qtype is the logical type.

[`transformer.c`](../../src/graph/transformer.c) imports retained target/draft
final-head summaries into that program once during cold binding normalization.
Those summaries remain persisted compatibility inputs, not an alternative
executable final-head implementation. No additional durable schema is introduced.
The CPU equations now belong to the backend operation; the previous graph-side
final/capture implementations and direct runtime final-kernel call are removed.
The existing CUDA numerical kernel is unchanged.
The numerical ABI is now in
[`neural_operations.h`](../../include/yvex/internal/neural_operations.h), separate
from model plans, import and runner records. CPU and the CUDA operation interface
consume that narrow contract; existing numerical type names/layouts are retained.
[`program_stage.c`](../../src/runtime/program_stage.c) binds tensor-only program
resources and host/device transport without building IR or selecting family
semantics. Input arity comes from the physical entrypoint, not a single-input
runner assumption. Each input has an explicit typed device argument or its own
host staging view; all staging and descriptor storage is admitted at open, with
no argument-array allocation during invocation. Host publication waits for all
result transfers and cancellation checks; device-result publication generations
remain the enclosing runtime's authority. The full-evidence CPU reference path
consumes the same compiled operation through a CPU backend. This operation is
one node in the compiler-owned DeepSeek target/draft schedule; the physical
stage does not own or reconstruct that schedule.

Target features consumed by draft execution use `tensor.stream_mean`, a pure
F32 `[rows, streams, width]` to F32 `[rows, width]` reduction. Ordered stream
addition and division use F64 accumulation, followed by one F32 publication;
there is no BF16 rounding. The verifier owns the static stream/channel geometry
and matching row population. `stream_mean.f32.f64acc.v1` selects the existing
CUDA reduction kernel or its CPU numerical implementation. Cold binding
normalization imports the retained geometry into a parameter-free feature
program; runtime no longer computes that mean or calls its kernel directly.
Feature-layer selection remains runner composition. Strided host/device feature
storage is a publication destination, not part of the reduction semantics.

`nn.linear_residual` represents projection plus an explicit residual **before**
the result's low-precision publication. It is not interchangeable with BF16
`nn.linear` followed by `tensor.add`: that decomposition introduces an extra
rounding point. Its verifier requires the residual's exact result type and row
population; `linear_residual.bf16.f32add.v1` admits BF16 parameters/inputs,
adds the residual to the F32 projection, and rounds once to BF16. CPU uses the
existing scalar dot owner; CUDA uses the existing encoded projection with its
additive operand and BF16 publication. The bounded independent oracle includes
`1.001953125 - 1 = 0.001953125`, which double rounding would replace with zero,
plus cancellation, nonfinite refusal and allocation cleanup. This is a
qualified operation used by the compiled MiniMax text component; it does not
qualify the remaining vision, latent or VAE computation.

### Compiled text components

[`text_program.c`](../../src/graph/text_program.c) imports source-declared dense
text geometry into explicit embedding, grouped RMS normalization, Q/K/V
projections, positional tables, attention, residual and gated-FFN operations.
The 50-block structural probe lowers to 1,303 physical instructions with 11
reusable storage slots. Runtime binds exact encoded parameter handles once;
it does not walk a parallel text layer plan. The importer recipe is a cold
source interpretation, not an executable backend descriptor.

Text and vision share a cold parameter-directory binder driven by the physical
program's parameter IDs. Text invocation carries no recipe, layer count,
embedding name or layer-role callback. Source parameter naming stays in the
importer; the product adapter receives that linkage with its compiled program.
The vision source recipe and parameter-name interpretation also stay in the
architecture importer. After product image preparation establishes the exact
grid, an explicitly supplied cold compiler entry verifies and lowers that
population before component resource binding. The product adapter cannot fetch
a recipe from the family registry or replace the source projection. Missing
compiler entry refuses before preprocessing or output publication; neural
execution still uses the common physical SSA consumer.
Runtime validates result geometry against the compiled signature, then executes
exact resident views. The separate multimodal text request/dispatcher and
backend text weight-role enum are removed; multimodal tensors extend the same
invocation. Missing bindings, invalid populations and resource budgets refuse
without output publication.

Multimodal inputs retain separate position streams, visual rows and deep-stack
contributions. `tensor.masked_rows` makes initial replacement and per-block
addition explicit dependencies; inactive rows remain bit-identical. Its
currently admitted implementation uses bounded host staging, preserving the
previous exact behavior rather than claiming a new GPU acceleration.
`tensor.rotary_tables` owns the declared interleaved position policy;
`tensor.rotary_half` preserves the distinct product/publication rounding points.
`attention.full` consumes a complete admitted sequence, optionally causal; it
does not imply retained-prefix or cross-request state support.

Compiler verification rejects malformed groups, populations, scalar/result
types and attributes before lowering. Artifact-backed first-layer evidence and
bounded two-layer before/after preservation are different evidence classes;
neither is complete multimodal model or upstream family qualification.

### Computed index values

`tensor.index_linearize` represents exact bounded Cartesian coordinates as an
SSA index stream: `major * minor_extent + minor`. The semantic verifier checks
types, equal populations and nonempty nonoverflowing domains. Physical lowering
admits `index_linearize.host.u32.v1` only when the complete domain fits U32;
larger domains fail closed rather than converting through floating point.
These intermediate values use compiler-planned, last-use-reusable host slots
separate from device activations, with explicit produced/unpublished state and
aggregate host-budget accounting. They can feed other index operations and
indexed tensor consumers. External index-valued results and general device
index production are not claimed by this implementation.

The joint component's `step` entrypoint consumes raw modality tags and timestep
indices, then computes their table coordinates in the compiled program. Runtime
no longer computes that formula or reads a joint source recipe: input/result
geometry comes from admitted signatures, while partition/capacity validation,
resource retention and output publication remain runtime responsibilities.
Prepared-resource identity uses the compiled prepare/step identities and exact
runtime inputs, not a second copy of the source architecture.

Every admitted physical operation also declares which operand positions consume
encoded parameters and which require materialized values. Logical tensor type
compatibility alone is insufficient: an encoded constant has no activation slot.
The compiler and binary-admission verifier refuse an unsupported storage-class
combination before execution, including a constant substituted for a linear
input/residual or a runtime activation substituted for an immutable weight.
This does not prohibit such logical programs; they need an explicit constant
materialization or another admitted implementation, neither inferred by runtime.
Each operation consumes its compiler-verified row population, not an assumed
copy of the entrypoint population. Fixed leading extents remain fixed; the
single admitted dynamic row symbol resolves only at invocation. Linear
specializations are keyed by population as well as channel geometry.
`tensor.reshape` preserves the proven element product and precision; unrelated
symbols with equal bounds do not become equivalent. The CPU/CUDA population
fixture executes 8 → 4 → 2 rows through distinct projections, including binary
reimport and repeated invocation. Multiple independent dynamic row symbols
still require additional lowering.

The bounded operator command lifecycle lives in
[`transformer_operator.c`](../../src/runtime/transformer_operator.c), separate
from engine/session computation. None of these changes migrates the remaining
attention/MoE topology or claims independent whole-model conformance.

`mhc.residual_pre` makes BF16 residual streams and F32 affine mixing results
explicit operands, alongside immutable scale/base parameters. It returns three
values: a BF16 collapsed row, F32 post gates and an F32 source-to-target matrix.
The verifier owns row identity, stream/channel geometry, three-scale geometry,
positive epsilons/multiplier and Sinkhorn iteration count. Physical lowering
selects `mhc.residual_pre.bf16.v1`; CPU and CUDA execute the same admitted
operation interface. The retained CUDA kernel uses prepared scratch because
it rounds residual storage in-place: a pure operation must not modify a borrowed
SSA operand. Scratch preparation follows the actual invocation population,
not the model's symbolic context horizon; growth accounts replacement overlap
and preserves the old allocation if the new budget cannot admit it.

Cold binding normalization compiles each MoE ingress into explicit reshape,
encoded affine projection, precision conversion, `mhc.residual_pre`,
`nn.weighted_rms`, exact BF16-value expansion to F32 and router projection.
CPU and CUDA runtime consumers invoke that program
through the common stage executor; the former scalar and CUDA ingress
composition paths are removed. `nn.weighted_rms` declares BF16 input/result, F32 logical weights and
F64 epsilon/inverse/scaling. Physical lowering separately admits F32 or BF16
encoded weights. It preserves CPU's source-order F64 reduction and CUDA's
retained 256-lane F32 reduction with F64 overflow recovery, then F32-to-BF16
publication. It is not interchangeable with the ordinary F32-epsilon RMS
implementation. These are explicitly different backend numerical contracts,
not a claim of whole-model CPU/CUDA agreement.

The optional `nn.linear` reduction obligation `YVEX_IR_REDUCTION_ROW_DOT`
lowers to `linear.row_dot.f32.v1`. Input precision and reduction selection are
independent: this logical F32 operation admits F32 and BF16 encoded parameters
and retains the encoded row-dot implementation,
not a matrix-library reassociation. Other linear programs keep their admitted
matrix implementations. The physical identity binds this choice; incompatible
parameter precision or an implementation that drops the obligation fails closed.

Session-owned stages bind ingress parameters at cold admission and retain
independent normalized/gate/mixing/router-logit result carriers. The CUDA
selection/expert consumer takes those four typed operands; it neither binds the
five ingress/projection weights nor allocates the old affine/gate scratch.
CPU selection likewise consumes compiled logits instead of projecting weights.
Compiled operands are dynamic
inputs to kernel replay, not captured addresses. The stage's parameter footprint
and router/expert work contribute separately to execution accounting.

MoE routing, experts and outer attention/target/draft orchestration remain
distinct admitted physical operations rather than one monolithic forward SSA
program. Their ordering and dependencies are owned by the canonical operator
graph retained in model-plan v8; this separation is not upstream conformance.

The retained router's correction addition is F32 before ranking, matching
the source Gate's F32 scores and bias. CPU, single-row CUDA and row-parallel
CUDA must share that precision boundary; widening only the single-row sum to
F64 changes rounded ties and can select different experts. Equal corrected
scores use YVEX's explicit low-ordinal ordering (not an upstream `topk` tie
guarantee). Routing weights use the uncorrected scores. The CUDA MoE lane
checks sub-ULP/visible bias, exact ties, ordered hash routing and malformed
selection/numerics independently of the model fixture. This bounded arithmetic
check is not official-vector or whole-model conformance.

`mhc.residual_post` makes residual, core result, post gates and source-to-target
mixing four explicit F32 operands with common row identity. Its pure computation
uses ordered F64 multiply/add, F32 conversion and BF16 round-to-nearest-even
publication. `mhc.residual_post.f64acc.bf16.v1` admits CPU arithmetic and the
existing CUDA residual kernel without changing its equations. Stream/channel
geometry, mixing orientation, result geometry and precision are compiler-verified.
The old `yvex_transformer_deferred_post` graph numerical API is removed. CPU and
full-evidence DeepSeek block execution invoke the compiled four-input program.
Normal and full-evidence CUDA execution call the device stage: MoE transfers its core result,
post gates and mixing matrix to three disjoint caller-owned carriers before
workspace reuse. The runtime slices admitted carrier populations; compatible
multi-session scheduling gathers/scatters all three results without rebuilding
their numerical meaning. The post stage consumes these plus the residual input.
Queued copies are not transaction commit; deferred status remains owned by the
existing MoE phase completion and enclosing state transaction.

The engine scheduler accepts matching input/result populations and has no
recombined-output destination or alternate fused-post branch. Device batches
always gather/scatter the three typed results; residual composition remains
the compiled post program's responsibility.

The internal MoE row-batch schema v2 removes the recombined destination from
the runtime and CUDA operation contract too. Older row schemas fail before
layout-dependent reads. Batched expert execution publishes only core, gates
and mixing; the separate typed residual program consumes them after successful
completion. This is an internal call-record change, not a wire or artifact
schema change. Standalone encoded projections own their temporary status and
packing storage and cannot consume or rewind an enclosing operation's arena.

Full evidence retains a separate CPU reference stage alongside device execution;
it does not choose a different device composition. Both the single-row and
batched backend fused-post branches and their recombined-output fields are
removed; direct numerical fixtures also consume the typed residual program.
The canonical schedule owns where this program follows MoE work. The
standalone post still has per-operation CUDA synchronization; no speedup is
claimed.

Production token-forward execution and the independent BF16 CUDA fixture both
use [`program_device.c`](../../src/runtime/program_device.c) with
[`program_kernels.c`](../../src/runtime/program_kernels.c). The older bounded
FFN executor and its private ABI have been retired. Model-plan v5 serialization
retains an import-compatibility owner, not a parallel runtime. Parameter
representation is checked once at cold binding; an invocation supplies only
its actual input values, not a mutable directory of weight interpretations.
Borrowed device-result validity belongs to the enclosing producer publication
generation, not an IR value ID.

The independent CUDA fixture compares 256 scalar BF16 results at widths 1 and 3
with zero error/tolerance. It exercises allocation budgets, failed preparation,
failed execution/retry, alias/shape/representation refusal and return to baseline
allocated bytes after cleanup. These are bounded operator/lifetime proofs, not
full Qwen execution, upstream conformance or an architecture performance claim.

[`src/graph/program.c`](../../src/graph/program.c) owns the parameter join between
this program, Transformation IR and PEIR. Each constant binds an exact source
symbol through one identity-preserving transform to a physical terminal handle.
Missing/ambiguous realizations, wrong source, logical dtype/shape, tensor role,
row geometry, encoded storage or inadmissible physical class fail compilation.
Equal element count is not sufficient; a transpose is not silently treated as
an alias. Broader transformation legalization remains explicit future work.
The projection has a distinct identity bound to all three compiler inputs and
retains the immutable module without retaining source payloads. Two admitted
physical recipes may share semantic identity while having different physical
projection identities. [`tests/unit/program.c`](../../tests/unit/program.c)
checks that distinction, negative joins and lifetime after input closure.

[`tests/unit/ir.c`](../../tests/unit/ir.c) owns the construction, dominance,
state/effect, signature, region, identity, pass and binary refusal evidence.
The source-owned Mamba2 projection additionally checks real audited mixer
geometry and parameter roles. Its imported `forward` has logits plus separate
convolution/SSM state results. Explicit tokenizer/normalization obligations
survive canonicalization, and its source-to-deployment gate still returns
UNSUPPORTED. This is representability, not complete model numerics or A01
execution. The program's sequence-symbol bound is an inspection envelope, not
a qualified context capacity.

## Repository boundary

Model weights, source payloads, complete artifacts, runtime bindings,
registries, transformation outputs, and raw evidence stay outside the
repository. Tiny test fixtures are admitted only by their focused test owner.
