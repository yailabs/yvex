<!-- docs:metadata
title: Representation, Quantization and Artifacts
id: yvex.architecture.representation-artifacts
document: architecture-plane
status: mixed
owner: compiler
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: representation-artifacts
related: [yvex.architecture.artifacts-admission, yvex.architecture.deployment-specialization]
-->

# Representation, Quantization and Artifacts

**A logical model can have several exact physical representations.**

[Up](README.md)

Quantization changes how values are represented. It does not redefine the
model's logical role graph. GGUF is an admitted physical container, not the
identity of the logical model.

## From policy to bytes

Source roles and numerical constraints → physical policy/quant plan → encoded
tensor layout → deterministic artifact writer → admission.

| Choice | Boundary |
| --- | --- |
| Tensor qtype, packing, layout and scale companions | Physical representation and artifact identity |
| Exact source-derived operation/numerical meaning | Model semantics and compiler |
| Admitted implementation class and machine budget | Deployment specialization |
| Equivalent warp, tile and stream mechanics | Backend |

Calibration and importance matrices must retain their source identity. A prior
from a different checkpoint is a prior, not calibration of the new checkpoint.

## Current machinery and target search

Current quant planning, encoding, writer and admission machinery can construct
selected representations. Automatic sensitivity search, Pareto selection and
hardware-aware recipe recommendation remain the
[Physical Model Compiler research target](../research/physical-model-compiler.md).

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

## Executable weight preparation boundary

Storage encoding, executable layout and residency are independent decisions.
Registering or warming a GGUF mapping does not repack its quantized blocks for
a matrix kernel. Conversely, an aligned/plane-separated representation may be
prepared without changing decoded values; it still needs a compatible admitted
consumer, explicit storage and a lifetime. Preparation is not automatically a
speedup and a numerically different consumer is not an equivalent layout change.

For the selected computational reconstruction, the next admissible design is
**selective or replacement preparation**, not an unconditional second complete
resident weight copy. This is a design constraint, not an implemented new mode.
The compiler must describe legal parameter geometry and numerical obligations;
deployment must select and identity-bind the compatible executable realization;
the backend constructs/executes its buffers within engine-owned lifetime.
Do not put device pointers, current free memory or live kernel policy into
package PEIR v5. A persisted transformed package needs its own physical identity;
an engine-local compatible layout belongs to the sealed specialization and its
resource report, not a second semantic model.

Before admitting such a realization, retain the source terminal/representation,
layout/version, numerical class, consumer geometry, alignment and legal widths;
construction scratch and time; retained prepared bytes; overlap with source
backing during construction; and release/refusal behavior. Replaced spans must
not retain a compulsory resident duplicate. File-backed pages remaining in cache
are not automatically such a duplicate, nor are they automatically reclaimable
under a particular registration. Account for the actual backing and driver path.
Publication follows completed preparation and validation; partial construction
must unwind without invalidating the authenticated artifact or another engine.

The current 0731 Q2_K gate/up/down artifact does not satisfy the existing CUDA
matrix-triplet selector, which requires IQ2_XXS gate/up and Q2_K down. Repacking
alone therefore cannot select that existing class. A generic Q2_K-compatible
implementation needs explicit admission and numerical/full-model evidence;
changing storage to calibrated IQ2 is instead a new physical variant with
checkpoint-matched calibration and quality obligations. Neither alternative is
qualified by the present memory audit. See the
[dated admission boundary](../evaluation/retained-observations.md#0731-memory-audit-and-operator-reboot-boundary-2026-10-10).


## Implementation and evidence

[src/gguf](../../src/gguf) · [src/model/compilation](../../src/model/compilation) · [include/yvex/qtype.h](../../include/yvex/qtype.h) · [include/yvex/quant.h](../../include/yvex/quant.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Artifact and Package Admission](artifacts-admission.md) · [Deployment and Specialization](deployment-specialization.md)
