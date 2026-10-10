<!-- docs:metadata
title: Physical Model Compiler Research
id: yvex.research.physical-model-compiler
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Physical Model Compiler Research

**Which reproducible representation best satisfies workload, quality and hardware constraints?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

The selected `V010.PHYSICAL.MODEL.COMPILER.GOAL.DRIVEN.0` begins implementing
this target through the [native bounded search](../architecture/compiler-ir.md#goal-constrained-physical-search-partial-implementation).
Its initial policy synthesis and static refusal do not complete the research
target's measured optimization, preparation or independent qualification loop.

## Question and qualification

Which reproducible representation best satisfies workload, quality and hardware constraints?

A search must produce an admissible recipe, compare held-out quality and end-to-end resource/performance evidence, and beat a declared control without relaxing the computation.

## Questions rescued from the temporary ledger

The former research bootstrap supplied questions, not completed experiments.
The current [deployment](../architecture/deployment-specialization.md) and
[representation](../architecture/representation-artifacts.md) owners answer
where choices live; the automatic search/recommendation layer remains open.

| Research distinction | Question to resolve |
| --- | --- |
| Inventory / topology | Which resources exist, and how are they connected? |
| Capability / calibration | Which classes are legal, and what has this machine measured? |
| Profiling / telemetry / evidence | What observation was made, under which trustworthy basis? |
| Recipe / deployment | Which decisions change bytes, and which specialize an existing artifact? |
| Model Library / recommendation | What exists versus what representation should be built? |
| Resource catalog / residency policy | What can be represented versus what is automatically selected or evicted? |

Recipe candidates include per-role qtypes, grouping, scales, layout, packing,
alignment, sharing, transformations, state representation, placement, reserve,
calibration and quality constraints. Classify each as a semantic fact, artifact
choice, deployment constraint, equivalent backend detail, mutable state,
measurement or search metadata before adding an identity field.

The current capacity request consumes one selected hardware profile. Multi-GPU,
heterogeneous hosts, NVMe parameter tables and distributed topology remain
pressures to investigate. Describable, plannable, buildable, admissible,
executable and qualified are separate stages. Recommendation may suggest;
admission proves its own bounded claim.


## Physical Model Compiler

**Quantization ⊂ representation synthesis ⊂ physical model compilation.**
Program P is not a GGUF picker or merely a quantizer. It should turn immutable
source plus an execution objective into a reproducible physical recipe whose
quality, resource use and performance are measured together.

That future recipe coordinates three coupled but separately owned decision
spaces: parameter/package representation, physical computational realization
and target choice, and deployment/resource realization. Program P may search
or coordinate candidates across those spaces, but their identities remain
distinct. It does not absorb model semantics, artifact admission, runtime
residency or backend-local equivalent implementation choices.

The implemented deterministic parameter/package lane is verified source →
family semantic projection → Transformation IR → transform binding/artifact
lowering → quant plan/physical variant → artifact construction → admission and
materialization → PEIR package truth. It joins the separately lowered symbolic
computational program to construct a parameter-bound physical program before
the compiled model plan and runtime binding.
Semantic breadth remains partial. The missing strategic layer is coordinated
search/optimization across the separately owned physical decision spaces.

A future learned readout/probe/head is a first-class physical model object, not
an external classifier sidecar. Program P must bind its exact backbone/source
and parameter identity, dtype/qtype, layout, artifact provenance, physical
representation, compatibility, runtime placement and qualification evidence.
The selected V0 Decision Readout introduces no trainable weights and therefore
does not depend on this future learned-head lane.

The [profile resolution and experiment diagrams](../architecture/compiler-ir.md#profile-driven-compilation-context)
show the selected compiler ownership. Calibration/search data and independent
final evaluation remain different inputs throughout that loop.

This is a **target search architecture**, not an implemented automatic service.
Search may propose legal representations, admitted computational target choices
and deployment/resource candidates. Deterministic owners must still emit,
identify, admit and execute each selected result at its own boundary. Search may
not invent kernels, change source meaning, redefine artifact truth, take runtime
residency ownership or move backend-local placement authority into family
interpretation.

| Optimization dimension | Candidate decision space |
| --- | --- |
| Tensor precision | Dtype/qtype by role, layer or exact tensor |
| Group/channel geometry | Group size, channel policy, scale granularity |
| Scale representation | Encoding/precision of scales and auxiliary tensors |
| Layout | Physical tensor layout and backend-compatible ordering |
| Packing | Encoded blocks and packing strategy |
| Alignment | Physical alignment constraints |
| Sharing | Physically shared source/derived representations |
| Transformation | Admissible source-to-terminal transformation |
| Backend implementation | Only implementations compatible with the numerical representation |
| Workload reserve | Capacity left for state, context, workspace and concurrency |
| Conditional parameter tables | Row-addressable dtype/qtype, block geometry, scales, packing, alignment, partitions/shards, file-backed or mmap-compatible layout, device gathers and cacheable rows |
| Executable resource classes | Dense weights, routed experts and conditional tables may have different residency windows proven by program-phase dependencies; resident and non-resident representations remain distinct |
| Runtime-state representation | Semantic state geometry independent of physical dtype/qtype, packing/scales/layout and backend implementation |

Filter infeasible candidates before expensive builds or trials:

| Feasibility constraint | Required check |
| --- | --- |
| Backend implementation availability | A representation without an executable kernel is not a candidate. |
| Hardware capability | Dtype/instruction support must satisfy the admitted implementation. |
| Artifact size | Meet storage and distribution constraints. |
| Runtime model working set | Weight bytes alone do not describe runtime fit. |
| Sequence state | Reserve actual state geometry; long context may dominate. |
| Workspace / activation arena | Account for representation-dependent temporary memory. |
| Context requirement | A smaller candidate cannot sacrifice required context. |
| Concurrency requirement | Single-request fit cannot establish a concurrent envelope. |
| Quality constraint | Reject recipes exceeding the admitted degradation budget. |

There is no universal best recipe: neither the smallest artifact nor the highest
token/s wins independently. Optimize several objectives subject to hard
constraints; retain nondominated choices and their tradeoffs.

| Recipe class | Primary objective |
| --- | --- |
| Quality-oriented | Minimize degradation under memory/performance constraints |
| Balanced | Trade quality, memory and throughput within the envelope |
| Memory-oriented | Minimize working footprint above a quality floor |
| Throughput-oriented | Maximize execution rate under quality/memory constraints |

The mature output binds source identity, workload/hardware envelope,
reproducible recipe, per-tensor decisions, search/calibration evidence, artifact
and independent qualification evidence. **Search/calibration evidence != final
qualification evidence**: data used to select a recipe cannot independently
qualify that same recipe for release. [Release doctrine][doctrine] retains that
gate; no performance target here is a measured result.


## Technique taxonomy

**Research references, not a YVEX capability catalog.** The inspection below is
dated 2026-10-10. Paper dates identify original publication where available;
repository revisions identify the implementation inspected, not an imported
dependency or an executed experiment. No external benchmark becomes YVEX
evidence. Code licenses below do not grant rights to checkpoints or datasets;
vendored components can have additional terms. No implementation was copied.

### Algorithms and their actual obligations

| Class / reference | Mechanism and required inputs | Physical consequences and qualification boundary |
| --- | --- | --- |
| Scalar RTN | Round weights on a specified grid with explicit scale, zero point, grouping and tie rule. Weight-only construction can avoid activation calibration. | RTN is an algorithm category, not one byte format or numerical contract. The native deterministic codecs are current producers; their qtype and decoder contracts, not the label RTN, determine legality. |
| [GPTQ, 2022](https://arxiv.org/abs/2210.17323) | Calibration activations supply approximate second-order reconstruction information; sequential weight quantization compensates remaining columns. | Ordering, group geometry and scale data affect layout and kernel consumption. A Hessian/reconstruction proxy is not held-out model quality. No YVEX GPTQ producer is currently admitted. |
| [AWQ, 2023](https://arxiv.org/abs/2306.00978) | Activation statistics guide channel scaling to protect salient weights, with weight quantization and clipping/search choices. | Scales must be folded through mathematically compatible operations or executed explicitly; a W4 kernel alone does not implement that transformation. No YVEX AWQ producer. |
| [HQQ, original technical article](https://dropbox.github.io/hqq_blog/) | Half-quadratic optimization of weight reconstruction without requiring a calibration corpus; scale/zero and grouping remain explicit. | Calibration-free does not mean error-free. The reference distinguishes axis choices that lack the same fast consumer; preserve that distinction in applicability. No YVEX HQQ producer. |
| [AutoRound / SignRound, 2023](https://arxiv.org/abs/2309.05516) | Optimize rounding and clipping with signed-gradient reconstruction over calibration examples. | Requires a differentiable reconstruction producer and its exact recipe. Exported format, reconstruction algorithm and inference kernel are separate identities. No YVEX AutoRound producer. |
| [GSQ, 2026](https://arxiv.org/abs/2604.18556) | Gumbel-Softmax optimization of scalar grid assignments and group scales, including GPTQ initialization and calibration reconstruction. | This means **Gumbel-Softmax Quantization**, not generic group scaling. An export compatible with an integer/GGUF format still requires exact encoding, calibration and independent quality qualification. No YVEX GSQ producer. |
| [SmoothQuant, 2022](https://arxiv.org/abs/2211.10438) | Move activation outlier difficulty into weights by an equivalent channel rescaling before quantization; activation ranges come from calibration. | A weight/activation transformation changes both sides of the operator. W8A8 instruction support, scale lifetime and activation rounding must be admitted together. Research only in Program P. |
| [QuaRot, 2024](https://arxiv.org/abs/2403.10082) | Orthogonal/Hadamard rotations reduce outliers using compatible model reparameterizations and online transforms. | Finite arithmetic, norm/activation compatibility, transformed weights and runtime transforms all need proof. A rotation cannot be moved through an arbitrary nonlinear or SSM operator. Research only. |
| [QuIP#, 2024](https://arxiv.org/abs/2402.04396) | Hadamard incoherence processing plus lattice codebooks and reconstruction. | Codebook decoding, scales, rotations and consumer kernels form a new representation. Nominal two-bit size cannot be compared as if it were Q2_K. Research only. |
| [AQLM, 2024](https://arxiv.org/abs/2401.06118) | Approximate weight groups as sums of learned codebook vectors; calibration optimizes reconstruction. | Codebook storage, indices and lookup/computation costs must enter resource/performance models. Codebook quality does not imply an admitted YVEX consumer. Research only. |
| [QTIP, 2024](https://arxiv.org/abs/2406.11235) | Trellis-coded quantization with incoherence processing and specialized decoding. | Trellis state, codebook generation and kernels are not interchangeable with scalar block decoders. Research only; no imported GPL implementation. |
| [RCO, 2026](https://arxiv.org/abs/2605.00649) | Riemannian constrained optimization of discrete group choices under a budget; evaluates a non-decomposable model objective. | It is not just a size-sort heuristic. A per-group proxy allocator cannot claim the same non-decomposable objective or measured quality. Program P may implement bounded allocation without claiming to implement RCO. |
| [Unsloth dynamic quantization](https://unsloth.ai/blog/dynamic-v2) | Checkpoint-sensitive mixed precision / selective protection informed by calibration and evaluation. | Layer, role and expert sensitivity can differ; preserve their actual populations. A project's Apache license does not prove a particular dynamic-generation recipe is published or reproducible. Research reference, not an admitted producer. |
| [SparseGPT, 2023](https://arxiv.org/abs/2301.00774) | Calibration-based second-order pruning/reconstruction, including structured patterns. | Zeros in a dense tensor are not sparse acceleration. Pattern, index storage and an exact sparse kernel are required; unsupported patterns refuse. No generic sparse-model admission follows. |
| Outlier / dense-plus-sparse methods | Separate exceptional values from a compressed dense base, e.g. [SpQR, 2023](https://arxiv.org/abs/2306.03078). | Sparse indices, gathers, accumulation and peak memory must be counted. Research only; the referenced paper does not license uninspected code. |
| Low-rank factorization | Replace an operator by admitted factors or an explicit residual correction, selected by a source-bound approximation objective. | Changes graph shape, parameter lineage, launch/activation costs and often arithmetic. No general producer or model-preservation claim is selected here. |
| Activation / runtime-state representation | Quantize live activations, KV or recurrent state under a workload-specific numeric policy. | Unlike weight-only changes this affects session lifetime, commit/rollback and error accumulation. Recurrent state cannot inherit Transformer KV tolerances. Reuse numerical and state owners; no blanket admission. |
| Executable packing / [Marlin, 2024](https://github.com/IST-DASLab/marlin) | Kernel-native weight ordering and parallel work organization for admitted quantized matrix execution. | Storage bytes may remain unchanged while engine-local layout changes. Hardware instruction, alignment and dtype assumptions must be checked on GB10, not inferred from CUDA availability. No external kernel imported. |
| IO-aware attention / [FlashAttention, 2022](https://arxiv.org/abs/2205.14135) | Tiled attention reduces intermediate IO while preserving the mathematical operation. | Fusion/scheduling changes may alter finite reduction order. DeepSeek compressed attention is not automatically interchangeable with dense attention. Independent arithmetic and state qualification remains mandatory. |
| Hardware autotuning / cost models | Bounded search over legal implementation, tile, phase width, fusion and placement choices; measures actual hardware. | Component winners need full-model validation. Costs retain units, calibration hardware/workload and uncertainty; launch counts and overlapping waits cannot be added as independent latency. |
| Phase-aware memory / preparation | Assign liveness, construction/recomputation, storage tier, overlap and release from the physical program. | Residency is not layout preparation. Charge temporary overlap and canonical reserve; warming experiments already rejected on GB10 are not a default new search dimension. |
| QAT / training-dependent methods | Optimize model/adaptation parameters under quantization effects using differentiable execution. | Separate future [native adaptation](model-adaptation.md) boundary with recipe/dataset/checkpoint lineage. This compiler continuation does not implement a trainer or treat lower training loss as qualification. |

### Inspected implementation provenance

These immutable references support method inspection only. Dates for GSQ/RCO
above follow their original 2026 preprints, not the dates of downstream videos.
HQQ's linked original article is the authority; a formal paper date is not
invented where the project cites a technical article instead.

| Reference | Inspected revision | Repository-declared license |
| --- | --- | --- |
| [GPTQ](https://github.com/IST-DASLab/gptq/tree/2d65066eeb06a5c9ff5184d8cebdf33662c67faf) | `2d65066eeb06a5c9ff5184d8cebdf33662c67faf` | Apache-2.0 |
| [AWQ](https://github.com/mit-han-lab/llm-awq/tree/d6e797a42b9ef7778de8ee2352116e0f48a78d61) | `d6e797a42b9ef7778de8ee2352116e0f48a78d61` | MIT |
| [HQQ](https://github.com/dropbox/hqq/tree/d88a488ec8aa2d58362ef2038a52bca862db2e74) | `d88a488ec8aa2d58362ef2038a52bca862db2e74` | Apache-2.0 |
| [AutoRound](https://github.com/intel/auto-round/tree/7a660e16d39114958a035eab920a44fee0b0b8c4) | `7a660e16d39114958a035eab920a44fee0b0b8c4` | Apache-2.0 |
| [GSQ](https://github.com/IST-DASLab/GSQ/tree/03fc16484c369e3127225615d5e03e8d3a6043e3) | `03fc16484c369e3127225615d5e03e8d3a6043e3` | Apache-2.0 |
| [RCO](https://github.com/IST-DASLab/RCO/tree/9a1e09c07d468109cbe60a1b87d5036034a79d10) | `9a1e09c07d468109cbe60a1b87d5036034a79d10` | Apache-2.0 |
| [QuaRot](https://github.com/spcl/QuaRot/tree/5008669b08c1f11f9b64d52d16fddd47ca754c5a) | `5008669b08c1f11f9b64d52d16fddd47ca754c5a` | Apache-2.0 |
| [SmoothQuant](https://github.com/mit-han-lab/smoothquant/tree/c61476d728e42ae0d8a35e7e78494edcac3237b5) | `c61476d728e42ae0d8a35e7e78494edcac3237b5` | MIT |
| [QuIP#](https://github.com/Cornell-RelaxML/quip-sharp/tree/1d8f873e9a2a8b86b12bb1064c312c5689b77d98) | `1d8f873e9a2a8b86b12bb1064c312c5689b77d98` | GPL-3.0 |
| [AQLM](https://github.com/Vahe1994/AQLM/tree/e79a896ed6656fe4ed06193d42d004e7d0bbdbb2) | `e79a896ed6656fe4ed06193d42d004e7d0bbdbb2` | Apache-2.0 |
| [QTIP](https://github.com/Cornell-RelaxML/qtip/tree/e90c6688c8dfae326a3a81b5eb032db7c6680ec0) | `e90c6688c8dfae326a3a81b5eb032db7c6680ec0` | GPL-3.0 |
| [SparseGPT](https://github.com/IST-DASLab/sparsegpt/tree/147d2159dc4f3e9f73e47b32c04d7b3708f44436) | `147d2159dc4f3e9f73e47b32c04d7b3708f44436` | Apache-2.0 |
| [Marlin](https://github.com/IST-DASLab/marlin/tree/1f25790bdd49fba53106164a24666dade68d7c90) | `1f25790bdd49fba53106164a24666dade68d7c90` | Apache-2.0 |
| [FlashAttention](https://github.com/Dao-AILab/flash-attention/tree/94e22c906678e5483fa0e9e24d8e787bc2c0ed4c) | `94e22c906678e5483fa0e9e24d8e787bc2c0ed4c` | BSD-3-Clause |
| [Unsloth project](https://github.com/unslothai/unsloth/tree/c538b0fb8f576d286e1fa331956885a0d0789aa5) | `c538b0fb8f576d286e1fa331956885a0d0789aa5` | Apache-2.0; not proof that each dynamic recipe is released |

### Promotion and experimental obligations

| Posture | Required evidence | Can Program P advertise an executable method? |
| --- | --- | --- |
| Research reference | Primary source, dated/versioned method, explicit assumptions | No |
| Candidate implementation | Native producer, typed applicability/refusal, deterministic lineage | Only as experimental, with missing gates visible |
| Qualified producer | Independent numerical controls and checkpoint-linked preparation evidence | Within that producer's exact representation contract |
| Deployable consumer | Exact compatible kernel/program, capacity, lifecycle and full-model evidence | Only for the admitted backend/hardware/workload |
| Recommendation | Comparable authenticated measurements plus independent quality floor | Best qualified in the examined population, never a universal optimum |

RCO-like global loss optimization and local multiple-choice allocation solve
different problems. An additive reconstruction proxy assumes separable group
costs; actual model degradation need not be separable. Even an exact optimum of
that proxy requires independent held-out validation. Byte feasibility can be
computed exactly for a sealed tensor plan while performance remains unknown.
This distinction is especially important for MoE: parameter bytes, active expert
population, routing coverage and shared-expert cost are different quantities.

[doctrine]: ../releases/doctrine.md

## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
