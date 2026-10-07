<!-- docs:metadata
title: "DeepSeek native chat / speculative / maximum"
id: yvex.evaluation.qualification.deepseek-native-reasoning-maximum-05
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-reasoning-maximum-05.json
-->

# DeepSeek native chat / speculative / maximum

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-reasoning-maximum-05.json)

Target identity: `6ed2117a57bd07188777bf3bf977d0fe5fce7fb132ced154747bc7f9df3a3518`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / chat.short/turn-0 | 1.47421 | s | 3 | 1.4031–1.66723 | 0.0711119 |
| request.client-complete / chat.short/turn-0 | 36.2745 | s | 3 | 36.1786–36.3777 | 0.0958678 |
| ttft.client-visible / chat.short/turn-0 | 4.14675 | s | 3 | 4.14114–4.48887 | 0.00561097 |
| ttft.server / chat.short/turn-0 | 2.73803 | s | 3 | 2.67244–2.82164 | 0.0655918 |
| reasoning.first.server / chat.short/turn-0 | 2.73803 | s | 3 | 2.67244–2.82164 | 0.0655918 |
| reasoning.first.client / chat.short/turn-0 | 4.14675 | s | 3 | 4.14114–4.48887 | 0.00561097 |
| final.first.server / chat.short/turn-0 | 28.2181 | s | 3 | 28.0587–28.258 | 0.0398761 |
| final.first.client / chat.short/turn-0 | 29.6216 | s | 3 | 29.5331–29.9256 | 0.0884359 |
| reasoning.phase-rate / chat.short/turn-0 | 7.78353 | token/s | 3 | 7.77601–7.82291 | 0.00751553 |
| final.phase-rate / chat.short/turn-0 | 7.82797 | token/s | 3 | 7.81895–8.06242 | 0.00902484 |
| decode.post-first.committed / chat.short/turn-0 | 7.96152 | token/s | 3 | 7.93635–7.99723 | 0.0251663 |
| prefill.uncached / chat.short/turn-0 | 47.7254 | token/s | 3 | 47.4092–48.365 | 0.316206 |
| prefill.wall / chat.short/turn-0 | 2.13723 | s | 3 | 2.10896–2.15148 | 0.0142547 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### chat.short/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 102 | token | 3 | 102–102 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 102 | token | 3 | 102–102 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 203 | token | 3 | 203–203 | 0 |
| Final content | 52 | token | 3 | 52–52 | 0 |
| Draft cycles | 50 | cycle | 3 | 50–50 | 0 |
| Draft forwards | 50 | forward | 3 | 50–50 | 0 |
| Proposed | 250 | token | 3 | 250–250 | 0 |
| Selected verification | 250 | token | 3 | 250–250 | 0 |
| Target verifications | 50 | verification | 3 | 50–50 | 0 |
| Accepted draft | 107 | token | 3 | 107–107 | 0 |
| Rejected draft | 143 | token | 3 | 143–143 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 50 | token | 3 | 50–50 | 0 |
| Per-sample mean accepted prefix | 2.14 | token | 3 | 2.14–2.14 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.07097 | s | 3 | 2.06514–2.14404 | 0.0058259 |
| Verification phase | 16.369 | s | 3 | 16.2622–16.4057 | 0.0366883 |
| Speculative commit phase | 7.14953 | s | 3 | 7.09659–7.17708 | 0.0275513 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | CHARACTERIZED | Three fresh native chat.short sessions; bounded 256 committed tokens; source-authored reasoning-to-final boundary reached; final output capacity-limited;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three fresh native chat.short sessions; bounded 256 committed tokens; source-authored reasoning-to-final boundary reached; final output capacity-limited;  |
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
| source_delta | NOT RETAINED |
| build | ef64f4cf0defe06720c6cf815640ba5c9522ca20838be535d3f5bdfcd4990e4e |
| executable | 037ba2bc36d68893a6389102294f3efb17923adc357138525d4f1f79faea9a76 |
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
| reasoning | maximum |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **reasoning.first.server**: server turn start to first source-classified reasoning token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **reasoning.first.client**: client dispatch including connect to first nonempty reasoning fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **reasoning.phase-rate**: source-classified reasoning tokens / server phase from prefill completion to reasoning boundary or decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-maximum-05/case-0/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Native protocol v24 candidate characterization, context4096/chunk512/explicit greedy; not the historical operator chunk64 configuration or chat terminal rendering.
- Same chat.short workload across reasoning modes; maximum keeps the source-authored longer instruction. Different policy/input populations are separate experiments, never averaged.
- Three completed capacity-limited 256-token turns. Reasoning terminator naturally reached; complete final answer and exact last-reasoning-to-first-final transition latency NOT MEASURED.
- Source/build/executable retained; built dirty-source delta unavailable in version JSON, so observer source capture is not substituted into target build provenance.
- Generated-token identity includes execution/state lineage, not just token IDs. Equal ordered fragment manifests establish published-content agreement only.
- Tiny prompt prefill is not a 2K/8K throughput gate. Channel phase rates are not sustained post-first-token channel rates.
- No independent admitted full-logit tolerance, original higher-precision quality comparison, release claim or 20/700 Task exit.
