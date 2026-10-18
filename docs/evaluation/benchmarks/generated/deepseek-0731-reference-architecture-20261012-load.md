<!-- docs:metadata
title: DeepSeek 0731 external CUDA architecture controls / load_seconds
id: yvex.evaluation.observation.deepseek-0731-reference-architecture-20261012-load
document: evaluation
status: current
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../data/deepseek-0731-reference-architecture-20261012-load.json
-->

# DeepSeek 0731 external CUDA architecture controls / load_seconds

**CHARACTERIZATION — one structured observation, with its original limits.**

[Benchmarks](../README.md) · [Source observation](../data/deepseek-0731-reference-architecture-20261012-load.json)

![DeepSeek 0731 external CUDA architecture controls / load_seconds](deepseek-0731-reference-architecture-20261012-load.svg)

| Measurement | Value | Unit | Samples | Scope |
| --- | ---: | --- | ---: | --- |
| control-prefill load | 132.604945737 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |
| no-d2r-prefill load | 140.654057489 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |
| no-dense-d2r-prefill load | 149.841686063 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |
| no-mmq-prefill load | 144.867736846 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |
| control-coding load | 164.70268954 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |
| no-graphs-coding load | 157.791945195 | s | 1 | Fresh external process, file warmed explicitly; not storage-cold. Observed API load, not YVEX load. |

## Context

Model: **DwarfStar DeepSeek V4 Flash 0731 release**. Backend: **external CUDA, pinned objects and standalone public C consumers**. Device: **one NVIDIA GB10; GPU-7659fe74-b7c2-6e3e-bf99-f66c430366bc; driver 580.159.03**.
Source commit: `9139e2ae58a41503968a500f36f75895c1ba63fc`. Source stability: **frozen**. Warm/cold: **mixed**.

Evidence pointer: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/reference-architecture-ablation-02; each raw/identity/resource receipt is hash-bound in runtime_configuration

## Limits

- External ablations are not same-artifact YVEX comparisons or independent model-quality qualification.
- DS4_CUDA_MMQ=0 changes the prefill tier; routed expert D2R remains active. It is not all-MMQ disabled.
- First request is separate; two subsequent 2K/coding samples are not three steady samples.
- Alternative reference reductions may differ numerically; throughput comparison does not certify equivalence.
- No cache eviction. File warming and lazy execution preparation are distinct.
- Source-level Entrpi and latest antirez inspection does not reproduce their self-reported benchmark numbers.

The [methodology](../methodology.md) defines comparability. A document import does not rerun this experiment.
