<!-- docs:metadata
title: YVEX Product Vision
id: yvex.product.vision
document: product
status: mixed
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Product Vision

**Make model support an evidenced source-to-execution lifecycle.**

[Up](README.md)

A readable checkpoint is not a supported computation. YVEX connects source
provenance, model interpretation, compilation, physical representation and
stateful execution so that each support claim has an identifiable boundary.

## Verified model execution

**Thesis:** support should bind exact source, semantics, representation,
deployment and behavior. A model name cannot carry that promise.

The earned foundation is authenticated source/package/binding admission and
bounded execution of distinct model classes. Independent whole-model behavior
and release evidence remain incomplete. Measure refusal correctness,
reproducibility and oracle coverage at each stage, not parser success alone.

## Compiler and runtime co-design

**Thesis:** runtime should execute admitted compiled facts instead of repeatedly
discovering a model's meaning. Shared ownership should survive family growth.

Current typed programs, sealed descriptors, parameter joins and authenticated
bindings support real executing consumers. Broader architecture/output/state
forms remain open. Success means admitting a new computational shape without
a second runtime or hidden family policy in generic code.

## Representation independence

**Thesis:** the logical model, quantized representation, artifact and deployment
are different choices with different identities.

Current transformation, quant planning, GGUF writing, admission and
specialization establish the foundation. Automatic physical-recipe search and
recommendation remain targets. Measure quality, build cost, working set and
complete execution together; a smaller file alone is not an improvement.

## Native computational state

**Thesis:** stateful inference needs explicit lifetimes beyond a stateless
decoder. Shared machinery must accommodate different state equations.

Attention, recurrent, draft and other transactional domains already retain
their own geometry. General persistent cognitive state, useful learned updates
and cross-execution realization remain research. Measure isolation, stale
refusal, reuse and independent usefulness separately.

## End-to-end efficiency with explicit failure

**Thesis:** optimize preparation, compilation, load, memory, prefill, decode,
generation and scheduling without weakening the requested computation.

Typed measurement, admitted CPU/CUDA classes and negative-path controls exist.
Apple Silicon Metal is an integrated early backend with independently qualified
device, storage and F32-embedding primitives. Its model-execution and performance
frontiers remain open under the same [backend contract](../architecture/backend-execution.md).
Competitive performance and release-wide behavior are not established. Evaluate
latency/throughput, memory, preparation and numerics under exact comparable
configurations; retain failed and unavailable results.

## YVEX and YAI

YVEX owns computational/model/runtime meaning. YAI organizes semantic work,
Case continuity and authority around replaceable intelligence. Efficient
computation can help that product without making YVEX a semantic memory owner.
Future W → E interoperation remains a research contract, not an existing API.

## Read next

[Requirements](PRD.md) · [Architecture](../architecture/README.md) ·
[Earned state](../project-control/STATUS.md) · [Evaluation](../evaluation/README.md)
