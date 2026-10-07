<!-- docs:metadata
title: "Hardware and backend evidence"
id: yvex.evaluation.qualification.hardware
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../methodology.md
-->

# Hardware and backend evidence

[Benchmarks](../README.md) · [Methodology](../methodology.md)

Every row links its complete context. Missing metrics are not zero; these rows do not establish cross-target comparability.

| Target | backend | hardware_model | device_count | topology | context | strategy | reasoning | Quality | Performance |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| [DeepSeek candidate — diagnostic whole-model CUDA submission profile](qualification-deepseek-candidate-compute-profile-12.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | target-only | none | BLOCKED | UNQUALIFIED |
| [DeepSeek candidate — explicit page warming, coding target-only native v24](qualification-deepseek-candidate-explicit-warm-coding-11.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | target-only | none | BLOCKED | CHARACTERIZED |
| [Candidate target-only: native C hash-table coding control](qualification-deepseek-candidate-native-coding-17.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | target-only | none | BLOCKED | CHARACTERIZED |
| [DeepSeek candidate — 2048 new prefill positions, engineering native v24](qualification-deepseek-candidate-prefill-2048-09.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | target-only | none | BLOCKED | CHARACTERIZED |
| [DeepSeek candidate — 512 new prefill positions, engineering native v24](qualification-deepseek-candidate-prefill-512-09.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | target-only | none | BLOCKED | CHARACTERIZED |
| [DeepSeek candidate — 8K repeat numerical refusal and scoped cleanup](qualification-deepseek-candidate-prefill-8192-refusal-09.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | target-only | none | BLOCKED | UNQUALIFIED |
| [DeepSeek candidate — CUDA host registration and OS residency diagnostic](qualification-deepseek-candidate-registration-residency-08.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | target-only | none | BLOCKED | UNQUALIFIED |
| [Device-owned ingress: controlled native coding, context 4K and chunk 512](qualification-deepseek-device-ingress-coding-23.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 4096 | target-only | none | UNQUALIFIED | CHARACTERIZED |
| [Device-owned ingress candidate: native speculative coding, context 32K and chunk 512](qualification-deepseek-device-ingress-speculative-coding-25.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 32768 | speculative | none | UNQUALIFIED | CHARACTERIZED |
| [DeepSeek HTTP default-stochastic characterization](qualification-deepseek-http-stochastic-characterization.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek independent coding continuation — high](qualification-deepseek-independent-coding-target-03-high.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 4096 | target-only | high | BLOCKED | UNQUALIFIED |
| [DeepSeek independent coding continuation — maximum](qualification-deepseek-independent-coding-target-03-maximum.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 4096 | target-only | maximum | BLOCKED | UNQUALIFIED |
| [DeepSeek independent coding continuation — none](qualification-deepseek-independent-coding-target-03-none.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 4096 | target-only | none | BLOCKED | UNQUALIFIED |
| [Installed DeepSeek native coding control, protocol 25](qualification-deepseek-installed-native-coding-21.md) | cuda | NOT RETAINED | NOT RETAINED | NOT RETAINED | 32768 | speculative | none | UNQUALIFIED | CHARACTERIZED |
| [DeepSeek native coding high — bounded reasoning refusal](qualification-deepseek-native-coding-high-bounded-refusal.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | high | BLOCKED | UNQUALIFIED |
| [DeepSeek native coding/high refusal and owned-session cleanup](qualification-deepseek-native-coding-high-cleanup.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | high | BLOCKED | UNQUALIFIED |
| [DeepSeek representative native corpus / target-only / none](qualification-deepseek-native-lineage-target-none-12.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | target-only | none | BLOCKED | CHARACTERIZED |
| [DeepSeek native operator baseline](qualification-deepseek-native-operator-baseline.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek native chat / speculative / high](qualification-deepseek-native-reasoning-high-chat-06.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | high | BLOCKED | CHARACTERIZED |
| [DeepSeek native chat / speculative / maximum](qualification-deepseek-native-reasoning-maximum-05.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | maximum | BLOCKED | CHARACTERIZED |
| [DeepSeek representative native corpus / speculative / none](qualification-deepseek-native-representative-none-09.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek native residency and launch diagnostics / speculative / none](qualification-deepseek-native-residency-diagnostic-15.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | UNQUALIFIED | UNQUALIFIED |
| [DeepSeek native chat / target-only / maximum](qualification-deepseek-native-target-maximum-07.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | target-only | maximum | BLOCKED | CHARACTERIZED |
| [DeepSeek native candidate characterization](qualification-deepseek-native-throughput3-candidate.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek paired greedy coding — HTTP OpenAI](qualification-deepseek-paired-greedy-http.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek paired greedy coding — native v24](qualification-deepseek-paired-greedy-native.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek coding / HTTP / speculative / none / lineage witness](qualification-deepseek-paired-lineage-http-none-13.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek representative native corpus / speculative / none](qualification-deepseek-paired-lineage-speculative-none-13.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 4096 | speculative | none | BLOCKED | CHARACTERIZED |
| [Published producer: native C hash-table coding control](qualification-deepseek-published-native-coding-16.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | speculative | none | BLOCKED | CHARACTERIZED |
| [DeepSeek isolated synthetic candidate - not local product](qualification-deepseek-throughput3-isolated-candidate.md) | cuda | NVIDIA GB10 | 1 | single-node single-device coherent unified memory | 32768 | target-only | none | BLOCKED | CHARACTERIZED |
