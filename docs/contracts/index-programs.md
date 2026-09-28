<!-- docs:metadata
title: Computed Index Contract
id: yvex.contracts.index-programs
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Computed Index Contract

**Typed computed addressing and the refusals that keep indexing bounded.**

[Up](README.md)

## Computed index values

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
encoded weights. CPU uses source-order F64 squares; CUDA uses a 256-lane F64
square reduction. Both retain F64 inverse/scaling and F32-to-BF16 publication.
The reduction orders remain distinct, so this does not claim general
whole-model CPU/CUDA agreement. The operation is not interchangeable with
ordinary F32-epsilon RMS.

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

The compiled `clamped_swiglu.f64math.bf16.v1` expert operation uses one
F64 nonlinear/product evaluation and one BF16 publication. The CUDA row,
grouped and matrix-tile consumers share that value rule; their dot-product
implementations may still have distinct admitted reduction error. Both CPU
and CUDA refuse non-finite gate/up operands before a clamp can mask them.
Component agreement does not establish whole-model CPU/CUDA agreement when
small early errors change a later routed expert population.

The decoded-F32 CUDA row-matvec path now accumulates its dot in source-order
F64 before F32 publication; Q8-activation and grouped/native rows retain their
separately admitted reductions. Full-evidence attention reduction uses the
CPU-compatible two-pass maximum and F32 destination accumulation; native
attention retains its online reduction. These are execution-class facts, not
a claim that the Q8/native production path is numerically identical to CPU.

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
geometry, parameter roles and source-owned token/numerical policy. Its imported
`forward` has hidden plus separate convolution/SSM successor-state results; its
`output` owns final normalization and vocabulary logits. The exact 64-layer
source compiles to common physical SSA, and artifact/engine admission is
qualified through the exact source-faithful artifact and binding. This is
internal exact-artifact execution plus first-layer independent component
evidence, not independent whole-model numerics, hosted conversation or release
support. The program's sequence-symbol bound is an inspection envelope, not a
qualified context capacity.
