<!-- docs:metadata
title: Runtime and Generation Horizons
id: yvex.research.runtime-horizons
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Runtime and Generation Horizons

**How can isolated state support fair, reusable and scalable execution?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

How can isolated state support fair, reusable and scalable execution?

Demonstrate real populations, bounded resources, isolation and failure recovery. Worker count, a multi-row kernel or an external runtime feature is not YVEX evidence.


## Paged typed sequence state target

A common physical page/block manager should serve attention KV, MLA/latent,
recurrent/SSM, convolution and speculative/candidate providers where paging is
appropriate. Fixed recurrent state need not grow like KV.

| Owner | Target responsibility |
| --- | --- |
| Common physical mechanism | Allocation, page ownership, reuse, eviction, movement and lifetime |
| Typed state provider | Semantic geometry, position mapping, updates and commit/abort rules |
| Session transaction | Coordinate publication or rollback across participants |

Existing virtual pages and immutable prefix sharing are foundations, not proof
of the complete cross-provider contract or an automatic caching policy.

## Future training resource execution

[Native adaptation](model-adaptation.md#native-computational-execution-target)
extends the existing compiler/physical/runtime/backend owners; no separate
training scheduler is selected. The OPEN phase-aware resource-lifetime target
may eventually cover forward, recomputation, backward and optimizer update with
explicit storage tiers and bounded prefetch/eviction. Frozen-base adapter
streaming, selective unfreezing and full training require distinct admission and
checkpoint contracts. No backward IR, optimizer ABI or training execution exists
by virtue of this research route.


## Speculation breadth

Candidate/verification/commit ownership is a meaningful foundation. The target
is one contract usable by independent draft models, model-native draft/MTP,
self-speculation, EAGLE-class strategies, n-gram and suffix proposals. These are
breadth probes, **not six scheduled deliveries**. ESTABLISHED would mean
materially different strategies share the lifecycle, not every algorithm exists.
An n-gram proposal strategy uses committed output/history to propose tokens for
verification under G. It is independent of lexical addressing and model-internal
n-gram parameter memory: speculation breadth does not establish Engram support.


## Mature-runtime comparison surface

Official [vLLM features][vllm], [TensorRT-LLM's compatibility matrix][trt] and
[MLC LLM deployment documentation][mlc] provide external reference surfaces.
These are not competitor scores or prescriptions for YVEX's internal design.
Feature combinations, model coverage and platforms have their own limits;
presence elsewhere does not establish YVEX feasibility.

| Mature-runtime capability | YVEX maturity area | Why it matters / primary reference |
| --- | --- | --- |
| Inflight / continuous batching | Sequence Runtime | Throughput/fairness; [TensorRT-LLM scheduling][trt-scheduling] |
| Paged sequence/KV management | Sequence Runtime | Dynamic memory; [TensorRT-LLM scheduling][trt-scheduling] |
| Prefix caching / KV reuse | Sequence Runtime | Repeated-prefix efficiency; [vLLM features][vllm] |
| Chunked prefill | Sequence Runtime | Long-prompt fairness/latency; [TensorRT-LLM scheduling][trt-scheduling] |
| LoRA/adapters | Dynamic Composition | Deployment flexibility; [vLLM features][vllm] |
| Structured/guided decoding | Generation Control | Constrained results; [vLLM structured output][vllm-structured] |
| Logprobs | Generation Control / Output | Token observability/scoring; [TensorRT-LLM outputs][trt-outputs] |
| Speculative decoding | Generation Control | Decode acceleration; [TensorRT-LLM matrix][trt] |
| Embedding/pooling/scoring | Output Runners | Beyond chat; [vLLM pooling][vllm-pooling] |
| Multimodal input | Multimodal Execution | Component inputs; [vLLM features][vllm] |
| Distributed execution | Scale-out | Multi-device scaling; [TensorRT-LLM matrix][trt] |
| Disaggregated serving | Scale-out | Split execution stages; [TensorRT-LLM matrix][trt] |

MLC's Python/REST/CLI and platform paths additionally pressure X's separation
of compilation, execution and consumer packaging. They do not establish YVEX
platform qualification. [MLC quick start][mlc]


[mlc]: https://llm.mlc.ai/docs/get_started/quick_start.html

[trt]: https://nvidia.github.io/TensorRT-LLM/features/feature-combination-matrix.html

[trt-outputs]: https://nvidia.github.io/TensorRT-LLM/features/additional-outputs.html

[trt-scheduling]: https://nvidia.github.io/TensorRT-LLM/features/paged-attention-ifb-scheduler.html

[vllm]: https://docs.vllm.ai/en/latest/features/

[vllm-pooling]: https://docs.vllm.ai/en/latest/models/pooling_models/

[vllm-structured]: https://docs.vllm.ai/en/latest/features/structured_outputs/

## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
