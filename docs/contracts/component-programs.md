<!-- docs:metadata
title: Component Program Contract
id: yvex.contracts.component-programs
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Component Program Contract

**Current spatial, dense, signal, joint and text execution contracts.**

[Up](README.md)

## Spatial and dense component programs

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

Prepared tensor execution backs its compiler-assigned F32 activation slots with
one allocation per program. Each slot retains a disjoint, capacity-checked typed
view and its original shape; liveness/reuse is still the compiler's decision.
Each activation slot starts at a 16-byte boundary, preserving the vector
alignment required by prepared linear execution. Rounded slot extents,
including bounded padding, are checked and included in the device budget
before allocation; there is no global pool. Index storage remains host-owned and completion and
prepared parameter resources retain separate lifetimes. Checked program close
releases only the backing owner, never a borrowed slot view. This changes
preparation allocation topology, not physical-program identity, arithmetic,
publication, state isolation, public ABI or wire semantics.

Device-only MoE ingress/shared stages bind their caller-owned device views and
do not reserve unused host-I/O input/result tensors. The CPU MoE path retains
its host spans; explicit host-I/O tensor stages remain available to their other
consumers. Resource reports count only resources actually owned by each path.

The device executor, tensor stages, prepared links and MoE result carriers use
this view rather than independently resolving shapes. Runtime still validates
actual backing, disjoint views and resource budgets. The view adds no persisted
schema, resource reservation or model-context claim.

The competitive execution candidate adds optional paired backend completion
hooks to a synchronous physical-program invocation. The prepared executor owns
the backend-declared byte workspace and accounts it against its device budget.
Only checked completion admits output views; a submitted intermediate value is
not a public result. Cancellation and operation refusal still discharge the
scope, while a failed device barrier makes the backend cleanup-only. Nested
scopes and release of borrowed completion storage refuse. This changes no
persisted physical-program identity, public C layout or protocol version; it
does not yet establish complete-model performance qualification.
Serialized predecessor checks are transferred into the owned completion word
without resetting their status. This is not nested scope admission. Compiled
output executables include that word in their aggregate admission budget;
neither a nonzero completion requirement nor a pending numerical failure may be
discarded as an implementation convenience.

The private invocation carries explicit completion ownership to its kernel
adapter. Pure copies into SSA-owned storage may enqueue only under that scope;
the paired completion drains them before output publication or resource reuse,
also after cancellation or operation refusal. Non-scoped invocation preserves
synchronous copying, and an unavailable queued-copy implementation retains its
known-correct synchronous path. This is not a relaxed public tensor-copy contract.

Encoded linear steps may borrow one program-owned packing buffer. Backend
geometry determines its byte requirement; the prepared program accounts its
maximum live extent and temporary growth overlap against the existing device
budget. Steps serialize buffer reuse on the same stream, and checked completion
precedes reuse by another invocation or release. No operation borrows an unrelated
attention arena. Standalone projections retain call-owned packing when no
workspace is supplied; input precision, reduction, status validation and output
publication remain unchanged. This is a private adapter, not a public ABI or
representation change.

## Joint preparation and executable composition

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


## Physical tensor execution and schema import

[`program_tensor.c`](../../src/graph/program_tensor.c) legalizes a bounded pure
tensor subset: BF16 linear with F32 accumulation, the two-rounding-point SiLU
product, and BF16 residual addition. Physical values distinguish encoded BF16
parameters from F32 activation storage carrying BF16 publications. Instructions
name admitted numerical implementations rather than a family or decoder kind.
Semantic identity, execution identity and physical-program identity remain
separate. Unsupported types, effects, operations, shapes or duplicate output
bindings refuse during compilation/import.

Compiled model-plan **v8** persists the physical token-forward and optional output
programs plus the canonical operator schedule inside runtime binding v17, without
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


## Compiled text components

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
