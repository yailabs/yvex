<!-- docs:metadata
title: Benchmarks
id: yvex.evaluation.benchmarks
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Benchmarks

**What ran, what was measured, and what the result does not prove.**

[Documentation](../../README.md)

Performance belongs to an exact checkpoint, representation, backend, machine,
configuration and workload. The tables below are generated from the same
receipts as the public target pages. Runtime benchmark v5 remains the producer
authority; documentation does not rerun or promote an experiment.

## Measured snapshot

<!-- docs:benchmark showcase -->

**DeepSeek-V4-Flash-0731 · NVIDIA DGX Spark GB10 × 1 · CUDA.**

One retained publication checkpoint, not a hardware-independent speed claim.
Native protocol measurements use an isolated resident host, a separate warmup
and fresh sessions without prefix reuse; they do not describe whichever engine is installed today.
File-cache state was uncontrolled. Full build, artifact, binding and specialization identities are linked per row.

| Configuration | Exact measured scope |
| --- | --- |
| Checkpoint / build source | `7872f01b1d1f` / `b8130fd51f9b`; full identities in each receipt |
| Physical representation | goal-v1-mxfp4-routed-q2_k |
| Input / execution | chunk=512; width=1; concurrency 1; reasoning `none` |
| Sampling / transport | Temperature 0, deterministic; `product-native-v25` |
| Driver / toolkit | 580.159.03; CUDA 13.0; nvcc 13.0.88 |

| Workload / metric | Strategy · context | Input / output | Median (tok/s) | Min–max | N |
| --- | --- | ---: | ---: | ---: | ---: |
| [C hash table](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010.md) · committed decode | target-only · 4096 | 31 / 256 | 9.66 | 9.64–9.66 | 3 |
| [C hash table](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010.md) · committed decode | speculative · 4096 | 31 / 256 | 14.38 | 14.37–14.43 | 3 |
| [Long text · 2K](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-2048-20261010.md) · new prefill | target-only · 16384 | 2048 / 16 | 103.24 | 101.95–103.31 | 3 |
| [Long text · 8K](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-8192-20261010.md) · new prefill | target-only · 16384 | 8192 / 16 | 89.73 | 89.61–89.81 | 3 |

Decode excludes the first committed token and its latency. Input/output counts are server-authored;
prefill counts newly executed input positions, not reused context.
Different strategies and context bands remain separate rows, not an averaged score.

| Coding request | Server TTFT | First visible content | Complete client request |
| --- | ---: | ---: | ---: |
| [target-only](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010.md) | 0.933 s | 1.461 s | 27.885 s |
| [speculative](generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010.md) | 1.231 s | 1.848 s | 19.574 s |

Performance: **CHARACTERIZED**. Independent representation quality: **BLOCKED**.
Latency cells are medians; their ranges, MAD, raw sample identities and memory/preparation
observations are in the linked receipts. No quality metric or confidence interval is invented.
These records do not establish high/maximum reasoning, other models, HTTP latency or release readiness.

<!-- /docs:benchmark -->

## Owners and reading paths

| Question | Read |
| --- | --- |
| Can these two numbers be compared? | [Methodology and comparison keys](methodology.md) |
| What exactly was tested? | [Checkpoint directory and exact targets](generated/qualification-index.md) |
| Which modes have evidence? | [Workload / reasoning matrix](generated/qualification-workloads.md) |
| What is the independent oracle? | [Reference captures](generated/qualification-references.md) |
| What remains an engineering target? | [GB10 execution gates](gb10-targets.md) |

## Observation projections

These earlier component, historical and synthetic observations are not the
current native-product benchmark. Their original scope stays attached.

- [DeepSeek GB10 inference-pipeline characterization](generated/deepseek-gb10-inference-pipeline.md)
- [DeepSeek GB10 ordered-dot characterization](generated/deepseek-gb10-ordered-dots.md)
- [fixture-publication](generated/fixture-publication.md)
- [mamba-readout-characterization](generated/mamba-readout-characterization.md)
- [qwen-readout-state](generated/qwen-readout-state.md)

[Observation schema](schema/observation.schema.json). Each projection links its source record.
