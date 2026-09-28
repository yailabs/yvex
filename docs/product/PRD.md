<!-- docs:metadata
title: YVEX Product Requirements
id: yvex.product.prd
document: product
status: mixed
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Product Requirements

**A native system for preparing, admitting and operating verified model execution.**

[Up](README.md)

## Users and core loop

An operator prepares and serves admitted models. An integrator consumes typed
results. A model engineer extends interpretation and compilation. A runtime
engineer qualifies execution and resource behavior. An evaluator examines exact
evidence rather than inferring support from a model list.

**Discover → acquire → verify → prepare → admit → load → execute → observe.**
Unload retires executable resources; eviction removes managed storage. They are
different operations. A host may remain healthy with no loaded engines.

## Required product boundaries

| Requirement | Owning technical contract |
| --- | --- |
| Exact source identity survives preparation | [Source and provenance](../architecture/source-provenance.md) |
| Unsupported semantics refuse before execution | [Family integration](../model-families/integration.md) |
| Artifact/deployment/lifetime identity stays distinct | [Architecture invariants](../architecture/INVARIANTS.md) |
| Sessions isolate mutable state and publish atomically | [Runtime contract](../contracts/runtime.md) |
| Clients consume typed behavior and errors | [Interfaces](../architecture/interfaces-protocols.md) |
| Support and performance claims carry appropriate evidence | [Evaluation](../evaluation/README.md) |

## Product surfaces

The `yvex` executable provides finite offline work, a persistent foreground host
and runtime clients. Installed C headers expose bounded integration contracts;
the local protocol and OpenAI adapter project host behavior. Restricted remote
identity/status management is a separate qualified bootstrap boundary.

Models share compilation/runtime machinery where their semantics permit it.
Each family retains exact source, role, state and numerical obligations. Product
interaction does not imply that every format, checkpoint, modality or backend is supported.

## Success and current limits

Success requires reproducible admitted computation, truthful refusal, isolated
state, inspectable lineage and evidence appropriate to the claimed workload.
Current [Status](../project-control/STATUS.md) separates operator availability,
engineering mechanisms, model/backend scope and qualification.

Automatic representation recommendation, general continuous batching,
distributed execution, universal model coverage and native cognitive state are
not current product promises. The product does not execute application tools,
own YAI Case semantics or make model output authoritative.

## Continue

[Vision](VISION.md) · [Quick start](../guides/quickstart.md) ·
[Selected delivery](../project-control/TASKS.md)
