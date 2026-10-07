<!-- docs:metadata
title: "DeepSeek native operator baseline"
id: yvex.evaluation.qualification.deepseek-native-operator-baseline
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-operator-baseline.json
-->

# DeepSeek native operator baseline

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-operator-baseline.json)

Target identity: `47b709776420f206218c1e39b1eaa8e8cfa24467a46bdd50018d1e5e7c69e78c`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| request.client-complete / chat.short/turn-0 | 98.6919 | s | 3 | 98.1606–98.9683 | 0.276316 |
| ttft.server / chat.short/turn-0 | 3.85064 | s | 3 | 3.84765–4.07067 | 0.00298902 |
| ttft.client-visible / chat.short/turn-0 | 4.95773 | s | 3 | 4.79589–5.09641 | 0.138673 |
| prefill.uncached / chat.short/turn-0 | 9.29702 | token/s | 3 | 9.19262–9.32415 | 0.0271293 |
| decode.post-first.committed / chat.short/turn-0 | 2.7158 | token/s | 3 | 2.71252–2.74008 | 0.00328511 |
| request.client-complete / coding.metal/turn-0 | 89.8376 | s | 3 | 89.562–89.9177 | 0.0800222 |
| ttft.server / coding.metal/turn-0 | 6.55607 | s | 3 | 6.52883–6.55862 | 0.00254935 |
| ttft.client-visible / coding.metal/turn-0 | 7.58457 | s | 3 | 7.41443–7.6748 | 0.0902325 |
| prefill.uncached / coding.metal/turn-0 | 10.2435 | token/s | 3 | 10.2274–10.2979 | 0.0160797 |
| decode.post-first.committed / coding.metal/turn-0 | 3.10064 | token/s | 3 | 3.10024–3.10422 | 0.00040541 |
| request.client-complete / conversation.coding/turn-0 | 4.40529 | s | 3 | 4.40027–5.03306 | 0.00501638 |
| ttft.server / conversation.coding/turn-0 | 2.53119 | s | 3 | 2.5281–3.11649 | 0.0030819 |
| ttft.client-visible / conversation.coding/turn-0 | 3.41395 | s | 3 | 3.41016–4.01528 | 0.0037939 |
| prefill.uncached / conversation.coding/turn-0 | 5.26324 | token/s | 3 | 3.57764–5.291 | 0.0277625 |
| decode.post-first.committed / conversation.coding/turn-0 | 9.07921 | token/s | 3 | 8.8465–9.09473 | 0.0155199 |
| request.client-complete / conversation.coding/turn-1 | 85.8486 | s | 3 | 85.7971–86.2626 | 0.0514572 |
| ttft.server / conversation.coding/turn-1 | 2.52765 | s | 3 | 2.5272–2.60362 | 0.000442296 |
| ttft.client-visible / conversation.coding/turn-1 | 2.52904 | s | 3 | 2.52903–2.6056 | 1.3241e-05 |
| prefill.uncached / conversation.coding/turn-1 | 7.64605 | token/s | 3 | 7.31723–7.67046 | 0.0244073 |
| decode.post-first.committed / conversation.coding/turn-1 | 3.06056 | token/s | 3 | 3.04819–3.06244 | 0.00188124 |
| request.client-complete / conversation.coding/turn-2 | 86.2541 | s | 3 | 86.1366–86.2802 | 0.0261239 |
| ttft.server / conversation.coding/turn-2 | 3.94943 | s | 3 | 3.94689–3.98231 | 0.00254014 |
| ttft.client-visible / conversation.coding/turn-2 | 3.95054 | s | 3 | 3.94785–3.98431 | 0.00269214 |
| prefill.uncached / conversation.coding/turn-2 | 10.7394 | token/s | 3 | 10.6476–10.7625 | 0.023042 |
| decode.post-first.committed / conversation.coding/turn-2 | 3.09962 | token/s | 3 | 3.09727–3.10277 | 0.00234911 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### chat.short/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 23 | token | 3 | 23–23 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 23 | token | 3 | 23–23 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 68 | cycle | 3 | 68–68 | 0 |
| Draft forwards | 68 | forward | 3 | 68–68 | 0 |
| Proposed | 340 | token | 3 | 340–340 | 0 |
| Selected verification | 339 | token | 3 | 339–339 | 0 |
| Target verifications | 68 | verification | 3 | 68–68 | 0 |
| Accepted draft | 118 | token | 3 | 118–118 | 0 |
| Rejected draft | 221 | token | 3 | 221–221 | 0 |
| Discarded draft | 1 | token | 3 | 1–1 | 0 |
| Correction/bonus | 68 | token | 3 | 68–68 | 0 |
| Per-sample mean accepted prefix | 1.73529 | token | 3 | 1.73529–1.73529 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 5.71804 | s | 3 | 5.70693–5.85971 | 0.0111082 |
| Verification phase | 60.543 | s | 3 | 60.165–60.6242 | 0.0812012 |
| Speculative commit phase | 28.141 | s | 3 | 27.7303–28.1577 | 0.0167494 |
### coding.metal/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 53 | token | 3 | 53–53 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 53 | token | 3 | 53–53 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 60 | cycle | 3 | 60–60 | 0 |
| Draft forwards | 60 | forward | 3 | 60–60 | 0 |
| Proposed | 300 | token | 3 | 300–300 | 0 |
| Selected verification | 298 | token | 3 | 298–298 | 0 |
| Target verifications | 60 | verification | 3 | 60–60 | 0 |
| Accepted draft | 136 | token | 3 | 136–136 | 0 |
| Rejected draft | 162 | token | 3 | 162–162 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | 60 | token | 3 | 60–60 | 0 |
| Per-sample mean accepted prefix | 2.26667 | token | 3 | 2.26667–2.26667 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 5.06257 | s | 3 | 5.05798–5.06971 | 0.00459427 |
| Verification phase | 53.5211 | s | 3 | 53.4461–53.5324 | 0.0112529 |
| Speculative commit phase | 24.2657 | s | 3 | 24.2449–24.2694 | 0.0036806 |
### conversation.coding/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 6 | token | 3 | 6–6 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 6 | token | 3 | 6–6 | 0 |
| Committed output | 10 | token | 3 | 10–10 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 10 | token | 3 | 10–10 | 0 |
| Draft cycles | 2 | cycle | 3 | 2–2 | 0 |
| Draft forwards | 2 | forward | 3 | 2–2 | 0 |
| Proposed | 10 | token | 3 | 10–10 | 0 |
| Selected verification | 10 | token | 3 | 10–10 | 0 |
| Target verifications | 2 | verification | 3 | 2–2 | 0 |
| Accepted draft | 7 | token | 3 | 7–7 | 0 |
| Rejected draft | 3 | token | 3 | 3–3 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 2 | token | 3 | 2–2 | 0 |
| Per-sample mean accepted prefix | 3.5 | token | 3 | 3.5–3.5 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 0.164018 | s | 3 | 0.163091–0.173621 | 0.000927731 |
| Verification phase | 1.78632 | s | 3 | 1.76769–1.83842 | 0.0186284 |
| Speculative commit phase | 0.41947 | s | 3 | 0.414099–0.422257 | 0.00278717 |
### conversation.coding/turn-1

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 25 | token | 3 | 25–25 | 0 |
| Reused prefix | 16 | token | 3 | 16–16 | 0 |
| New prefill | 9 | token | 3 | 9–9 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 61 | cycle | 3 | 61–61 | 0 |
| Draft forwards | 61 | forward | 3 | 61–61 | 0 |
| Proposed | 305 | token | 3 | 305–305 | 0 |
| Selected verification | 301 | token | 3 | 301–301 | 0 |
| Target verifications | 61 | verification | 3 | 61–61 | 0 |
| Accepted draft | 134 | token | 3 | 134–134 | 0 |
| Rejected draft | 167 | token | 3 | 167–167 | 0 |
| Discarded draft | 4 | token | 3 | 4–4 | 0 |
| Correction/bonus | 61 | token | 3 | 61–61 | 0 |
| Per-sample mean accepted prefix | 2.19672 | token | 3 | 2.19672–2.19672 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 5.13895 | s | 3 | 5.12729–5.15308 | 0.0116577 |
| Verification phase | 54.0968 | s | 3 | 54.0846–54.4079 | 0.0121858 |
| Speculative commit phase | 24.6391 | s | 3 | 24.639–24.7062 | 0.000125721 |
### conversation.coding/turn-2

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 308 | token | 3 | 308–308 | 0 |
| Reused prefix | 281 | token | 3 | 281–281 | 0 |
| New prefill | 27 | token | 3 | 27–27 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 57 | cycle | 3 | 57–57 | 0 |
| Draft forwards | 57 | forward | 3 | 57–57 | 0 |
| Proposed | 285 | token | 3 | 285–285 | 0 |
| Selected verification | 285 | token | 3 | 285–285 | 0 |
| Target verifications | 57 | verification | 3 | 57–57 | 0 |
| Accepted draft | 142 | token | 3 | 142–142 | 0 |
| Rejected draft | 143 | token | 3 | 143–143 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 57 | token | 3 | 57–57 | 0 |
| Per-sample mean accepted prefix | 2.49123 | token | 3 | 2.49123–2.49123 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 5.02297 | s | 3 | 5.01745–5.0337 | 0.00552577 |
| Verification phase | 52.4469 | s | 3 | 52.3652–52.4695 | 0.022525 |
| Speculative commit phase | 25.4571 | s | 3 | 25.4481–25.5155 | 0.00899272 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No inherited qualification;  |
| checkpoint-reference | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| representation-quality | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| backend-execution | UNQUALIFIED | Backend qualification is not inferred from a completed performance sample;  |
| deployment-performance | CHARACTERIZED | Three repetitions per representative native case; isolated host, retained operator binary;  |
| product-path | CHARACTERIZED | Three repetitions per representative native case; isolated host, retained operator binary;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | mixed-iq2xxs-q2k-mxfp4-v1 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 6c3e33a0ea61e9a371dc551d595b5a68e6f28e3059cde0ee49fc1370939c8350 |
| source_commit | 0408e2d32388f2a68e3135c71fb8f667bd41b135 |
| source_tree | 3ef25d0005ea29e259277fb34422041957fa133c |
| source_delta | NOT RETAINED |
| build | 7096d70f0be36025f7c712c5f025c996acdecc40d4bce08e5d34b6f1b766fa10 |
| executable | 69c921526508cf71b919b6143bfe423f14caa618c338c9706946cdc626f6920b |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | NOT RETAINED |
| context | 4096 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | deterministic product defaults; stochastic=0 temperature=1 top_p=1 typical_p=1 no explicit seed |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-0/observations.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-0/observations.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-0/observations.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-0/observations.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-0/observations.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-1/observations.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-1/observations.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-1/observations.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-1/observations.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-1/observations.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-operator-02/case-2/observations.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not a throughput Task closure, release benchmark, model-quality claim or chat-PTY performance claim.
- Same admitted artifact and checkpoint, but operator chunk=64 and candidate chunk=512 are distinct targets; source/build also differ.
- Old executable dirty source delta is unavailable. No source-clean causal speedup claim is inferred.
- A ten-token greeting is not sustained decode; 256-token coding is bounded decode evidence, not universal throughput.
- First publication and reasoning-to-final transition not measured; this target is reasoning none only.
- Independent output reference remains missing. Different generated token identities cannot be promoted to semantic equivalence.
- No uncached 2K/8K prefill, high/maximum reasoning, target-only or long-context qualification is imported.
