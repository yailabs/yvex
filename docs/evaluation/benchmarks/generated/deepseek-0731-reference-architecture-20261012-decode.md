<!-- docs:metadata
title: DeepSeek 0731 external CUDA architecture controls / sustained_committed_tokens_per_second
id: yvex.evaluation.observation.deepseek-0731-reference-architecture-20261012-decode
document: evaluation
status: current
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../data/deepseek-0731-reference-architecture-20261012-decode.json
-->

# DeepSeek 0731 external CUDA architecture controls / sustained_committed_tokens_per_second

**CHARACTERIZATION — one structured observation, with its original limits.**

[Benchmarks](../README.md) · [Source observation](../data/deepseek-0731-reference-architecture-20261012-decode.json)

![DeepSeek 0731 external CUDA architecture controls / sustained_committed_tokens_per_second](deepseek-0731-reference-architecture-20261012-decode.svg)

| Measurement | Value | Unit | Samples | Scope |
| --- | ---: | --- | ---: | --- |
| control-coding first | 17.880476676359763 | tok/s | 1 | 255 post-first committed tokens / post-first emit wall time; 256 total outputs; not speculation |
| control-coding subsequent | 19.608142307054173 | tok/s | 2 | 255 post-first committed tokens / post-first emit wall time; 256 total outputs; not speculation |
| no-graphs-coding first | 17.878990058996724 | tok/s | 1 | 255 post-first committed tokens / post-first emit wall time; 256 total outputs; not speculation |
| no-graphs-coding subsequent | 18.827159964233132 | tok/s | 2 | 255 post-first committed tokens / post-first emit wall time; 256 total outputs; not speculation |

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
