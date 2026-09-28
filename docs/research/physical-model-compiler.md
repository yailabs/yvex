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

```text
Immutable source: Safetensors + configuration + tokenizer
    → semantic model: architecture / tensor roles / topology
        + execution objective: task/workload, quality budget, context,
          latency/throughput, concurrency
        + target machine: backend/kernel support, memory hierarchy,
          compute capabilities, runtime reserve
    → representation space
    → sensitivity analysis + calibration evidence + feasibility filtering
    → candidate recipes → bounded candidate builds
    → numerical quality + runtime latency/rate + resource measurements
    → Pareto frontier → selected recipe
    → deterministic build → final artifact (currently GGUF)
    → independent final qualification
```

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


[doctrine]: ../releases/doctrine.md

## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
