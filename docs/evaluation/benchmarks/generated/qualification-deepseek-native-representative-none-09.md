<!-- docs:metadata
title: "DeepSeek representative native corpus / speculative / none"
id: yvex.evaluation.qualification.deepseek-native-representative-none-09
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-representative-none-09.json
-->

# DeepSeek representative native corpus / speculative / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-representative-none-09.json)

Target identity: `d23a11ae7394eef7697c6a372e2f043e28d3aea74b2bd386af9ad29c060c07f1`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / math.reasoning/turn-0 | 1.28844 | s | 3 | 1.2863–1.37607 | 0.00213952 |
| request.client-complete / math.reasoning/turn-0 | 28.3395 | s | 3 | 28.3261–28.3786 | 0.0134238 |
| ttft.client-visible / math.reasoning/turn-0 | 3.55542 | s | 3 | 3.4755–3.59696 | 0.0415325 |
| ttft.server / math.reasoning/turn-0 | 2.1871 | s | 3 | 2.17934–2.31066 | 0.00775981 |
| final.first.server / math.reasoning/turn-0 | 2.1871 | s | 3 | 2.17934–2.31066 | 0.00775981 |
| final.first.client / math.reasoning/turn-0 | 3.55542 | s | 3 | 3.4755–3.59696 | 0.0415325 |
| final.phase-rate / math.reasoning/turn-0 | 10.0855 | token/s | 3 | 10.0679–10.1159 | 0.0176046 |
| decode.post-first.committed / math.reasoning/turn-0 | 10.2895 | token/s | 3 | 10.2621–10.2905 | 0.000953648 |
| prefill.uncached / math.reasoning/turn-0 | 32.6318 | token/s | 3 | 32.4736–32.6757 | 0.0438335 |
| prefill.wall / math.reasoning/turn-0 | 1.65483 | s | 3 | 1.65261–1.66289 | 0.0022199 |
| admission.client / extraction.json/turn-0 | 1.29063 | s | 3 | 1.24572–1.39412 | 0.0449142 |
| request.client-complete / extraction.json/turn-0 | 10.6673 | s | 3 | 10.6276–10.7708 | 0.0397067 |
| ttft.client-visible / extraction.json/turn-0 | 3.55954 | s | 3 | 3.51394–3.65696 | 0.0456053 |
| ttft.server / extraction.json/turn-0 | 2.26824 | s | 3 | 2.26283–2.26891 | 0.000669713 |
| final.first.server / extraction.json/turn-0 | 2.26824 | s | 3 | 2.26283–2.26891 | 0.000669713 |
| final.first.client / extraction.json/turn-0 | 3.55954 | s | 3 | 3.51394–3.65696 | 0.0456053 |
| final.phase-rate / extraction.json/turn-0 | 12.6823 | token/s | 3 | 12.6673–12.6926 | 0.0103556 |
| decode.post-first.committed / extraction.json/turn-0 | 13.4978 | token/s | 3 | 13.4966–13.5079 | 0.00120396 |
| prefill.uncached / extraction.json/turn-0 | 34.7405 | token/s | 3 | 34.613–34.8263 | 0.0858023 |
| prefill.wall / extraction.json/turn-0 | 1.72709 | s | 3 | 1.72283–1.73345 | 0.00425506 |
| admission.client / writing.runbook/turn-0 | 1.31793 | s | 3 | 1.30408–1.34842 | 0.0138483 |
| request.client-complete / writing.runbook/turn-0 | 33.7014 | s | 3 | 33.6267–33.7026 | 0.00122217 |
| ttft.client-visible / writing.runbook/turn-0 | 3.37904 | s | 3 | 3.34292–3.39878 | 0.0197337 |
| ttft.server / writing.runbook/turn-0 | 2.05034 | s | 3 | 2.03885–2.0611 | 0.0107597 |
| final.first.server / writing.runbook/turn-0 | 2.05034 | s | 3 | 2.03885–2.0611 | 0.0107597 |
| final.first.client / writing.runbook/turn-0 | 3.37904 | s | 3 | 3.34292–3.39878 | 0.0197337 |
| final.phase-rate / writing.runbook/turn-0 | 8.307 | token/s | 3 | 8.30309–8.31129 | 0.00390848 |
| decode.post-first.committed / writing.runbook/turn-0 | 8.41561 | token/s | 3 | 8.40978–8.42081 | 0.00520074 |
| prefill.uncached / writing.runbook/turn-0 | 26.7348 | token/s | 3 | 26.4373–26.9858 | 0.250979 |
| prefill.wall / writing.runbook/turn-0 | 1.53358 | s | 3 | 1.51932–1.55084 | 0.014263 |
| admission.client / reasoning.schedule/turn-0 | 1.31333 | s | 3 | 1.29331–1.3248 | 0.0114661 |
| request.client-complete / reasoning.schedule/turn-0 | 30.776 | s | 3 | 30.7212–30.7932 | 0.0172012 |
| ttft.client-visible / reasoning.schedule/turn-0 | 3.35201 | s | 3 | 3.32697–3.35598 | 0.00396904 |
| ttft.server / reasoning.schedule/turn-0 | 2.03366 | s | 3 | 2.02722–2.04246 | 0.00643817 |
| final.first.server / reasoning.schedule/turn-0 | 2.03366 | s | 3 | 2.02722–2.04246 | 0.00643817 |
| final.first.client / reasoning.schedule/turn-0 | 3.35201 | s | 3 | 3.32697–3.35598 | 0.00396904 |
| final.phase-rate / reasoning.schedule/turn-0 | 9.15894 | token/s | 3 | 9.15627–9.17163 | 0.00266798 |
| decode.post-first.committed / reasoning.schedule/turn-0 | 9.29896 | token/s | 3 | 9.29442–9.30907 | 0.0045399 |
| prefill.uncached / reasoning.schedule/turn-0 | 26.4207 | token/s | 3 | 26.3309–26.6907 | 0.0897918 |
| prefill.wall / reasoning.schedule/turn-0 | 1.51396 | s | 3 | 1.49865–1.51913 | 0.00516281 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### extraction.json/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 60 | token | 3 | 60–60 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 60 | token | 3 | 60–60 | 0 |
| Committed output | 97 | token | 3 | 97–97 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 97 | token | 3 | 97–97 | 0 |
| Draft cycles | 15 | cycle | 3 | 15–15 | 0 |
| Draft forwards | 15 | forward | 3 | 15–15 | 0 |
| Proposed | 75 | token | 3 | 75–75 | 0 |
| Selected verification | 75 | token | 3 | 75–75 | 0 |
| Target verifications | 15 | verification | 3 | 15–15 | 0 |
| Accepted draft | 69 | token | 3 | 69–69 | 0 |
| Rejected draft | 4 | token | 3 | 4–4 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | 14 | token | 3 | 14–14 | 0 |
| Per-sample mean accepted prefix | 4.6 | token | 3 | 4.6–4.6 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 0.595774 | s | 3 | 0.595293–0.602983 | 0.000481503 |
| Verification phase | 4.89027 | s | 3 | 4.88788–4.90252 | 0.0023847 |
| Speculative commit phase | 2.0124 | s | 3 | 1.99884–2.0284 | 0.0135647 |
### math.reasoning/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 54 | token | 3 | 54–54 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 54 | token | 3 | 54–54 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 49 | cycle | 3 | 49–49 | 0 |
| Draft forwards | 49 | forward | 3 | 49–49 | 0 |
| Proposed | 245 | token | 3 | 245–245 | 0 |
| Selected verification | 241 | token | 3 | 241–241 | 0 |
| Target verifications | 49 | verification | 3 | 49–49 | 0 |
| Accepted draft | 158 | token | 3 | 158–158 | 0 |
| Rejected draft | 83 | token | 3 | 83–83 | 0 |
| Discarded draft | 4 | token | 3 | 4–4 | 0 |
| Correction/bonus | 49 | token | 3 | 49–49 | 0 |
| Per-sample mean accepted prefix | 3.22449 | token | 3 | 3.22449–3.22449 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.03778 | s | 3 | 2.01804–2.09132 | 0.0197369 |
| Verification phase | 15.7891 | s | 3 | 15.7518–15.8247 | 0.0355808 |
| Speculative commit phase | 7.08242 | s | 3 | 7.06149–7.10158 | 0.019163 |
### reasoning.schedule/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 40 | token | 3 | 40–40 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 40 | token | 3 | 40–40 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 53 | cycle | 3 | 53–53 | 0 |
| Draft forwards | 53 | forward | 3 | 53–53 | 0 |
| Proposed | 265 | token | 3 | 265–265 | 0 |
| Selected verification | 262 | token | 3 | 262–262 | 0 |
| Target verifications | 53 | verification | 3 | 53–53 | 0 |
| Accepted draft | 149 | token | 3 | 149–149 | 0 |
| Rejected draft | 113 | token | 3 | 113–113 | 0 |
| Discarded draft | 3 | token | 3 | 3–3 | 0 |
| Correction/bonus | 53 | token | 3 | 53–53 | 0 |
| Per-sample mean accepted prefix | 2.81132 | token | 3 | 2.81132–2.81132 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.21382 | s | 3 | 2.2024–2.21778 | 0.00395644 |
| Verification phase | 17.3239 | s | 3 | 17.3221–17.3254 | 0.00156037 |
| Speculative commit phase | 7.9217 | s | 3 | 7.8972–7.92598 | 0.00427678 |
### writing.runbook/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 41 | token | 3 | 41–41 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 41 | token | 3 | 41–41 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 59 | cycle | 3 | 59–59 | 0 |
| Draft forwards | 59 | forward | 3 | 59–59 | 0 |
| Proposed | 295 | token | 3 | 295–295 | 0 |
| Selected verification | 289 | token | 3 | 289–289 | 0 |
| Target verifications | 59 | verification | 3 | 59–59 | 0 |
| Accepted draft | 137 | token | 3 | 137–137 | 0 |
| Rejected draft | 152 | token | 3 | 152–152 | 0 |
| Discarded draft | 6 | token | 3 | 6–6 | 0 |
| Correction/bonus | 59 | token | 3 | 59–59 | 0 |
| Per-sample mean accepted prefix | 2.32203 | token | 3 | 2.32203–2.32203 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.42869 | s | 3 | 2.41757–2.44168 | 0.0111191 |
| Verification phase | 19.1042 | s | 3 | 19.045–19.1145 | 0.0102403 |
| Speculative commit phase | 8.74518 | s | 3 | 8.73527–8.80225 | 0.00991479 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | CHARACTERIZED | Repeated native representative cases; exact candidate configuration, not a universal rate or the historical operator defaults;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Repeated native representative cases; exact candidate configuration, not a universal rate or the historical operator defaults;  |
| representation-quality | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | 555963b7eb32e502bc46b7a3b384266329fafb55fb729eafb05a20722395ff67 |
| build | 9b8e28045285811c5623c6b0d6a8d0b1fc3d6340d87839877facd2b94a6a5753 |
| executable | 4a7686dc19a0931d516c4321cf6048d3cc071c01044846a96cb87604aafba7da |
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
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-0/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-1/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-2/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-representative-none-09/case-3/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Native protocol v24, not HTTP or terminal rendering. Isolated resident host, context4096/chunk512 and explicit greedy; not the historical operator chunk64 configuration.
- Individual workload rows are not averaged. Actual committed counts and early EOS remain visible; a short completion does not establish sustained decode.
- None mode only. High/maximum and target-only require separate target-bound evidence; reasoning capability is not inferred from the name of a math case.
- Tiny-input prefill does not establish 2K/8K throughput. First committed-fragment publication and exact reasoning-to-final transition timing remain unavailable.
- Built-source delta comes from the executable projection; observer snapshot uses the separate QA source-capture algorithm.
- No independently admitted full-logit tolerance or original higher-precision quality gate; not release performance or the20/700 Task exit.
