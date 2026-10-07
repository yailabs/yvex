<!-- docs:metadata
title: "DeepSeek native chat / speculative / high"
id: yvex.evaluation.qualification.deepseek-native-reasoning-high-chat-06
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-reasoning-high-chat-06.json
-->

# DeepSeek native chat / speculative / high

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-reasoning-high-chat-06.json)

Target identity: `f3c328e89a398a3e32d76f9c58aa9058bcb4226225e78797b4f0c00171b5d655`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / chat.short/turn-0 | 1.40423 | s | 3 | 1.37213–1.43063 | 0.0263999 |
| request.client-complete / chat.short/turn-0 | 33.3458 | s | 3 | 33.2596–33.3708 | 0.0250758 |
| ttft.client-visible / chat.short/turn-0 | 3.01438 | s | 3 | 2.99882–3.13034 | 0.0155611 |
| ttft.server / chat.short/turn-0 | 1.59473 | s | 3 | 1.58377–1.75863 | 0.0109669 |
| reasoning.first.server / chat.short/turn-0 | 1.59473 | s | 3 | 1.58377–1.75863 | 0.0109669 |
| reasoning.first.client / chat.short/turn-0 | 3.01438 | s | 3 | 2.99882–3.13034 | 0.0155611 |
| final.first.server / chat.short/turn-0 | 19.2483 | s | 3 | 19.2138–19.4557 | 0.0344985 |
| final.first.client / chat.short/turn-0 | 20.6527 | s | 3 | 20.6447–20.8277 | 0.00795963 |
| reasoning.phase-rate / chat.short/turn-0 | 8.46376 | token/s | 3 | 8.40081–8.48303 | 0.0192714 |
| final.phase-rate / chat.short/turn-0 | 7.95847 | token/s | 3 | 7.93783–8.12565 | 0.0206414 |
| decode.post-first.committed / chat.short/turn-0 | 8.4035 | token/s | 3 | 8.40088–8.46422 | 0.00262168 |
| prefill.uncached / chat.short/turn-0 | 21.7019 | token/s | 3 | 20.4688–21.8433 | 0.141445 |
| prefill.wall / chat.short/turn-0 | 1.05982 | s | 3 | 1.05295–1.12366 | 0.00686276 |

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
| Reasoning | 154 | token | 3 | 154–154 | 0 |
| Final content | 101 | token | 3 | 101–101 | 0 |
| Draft cycles | 36 | cycle | 3 | 36–36 | 0 |
| Draft forwards | 36 | forward | 3 | 36–36 | 0 |
| Proposed | 180 | token | 3 | 180–180 | 0 |
| Selected verification | 180 | token | 3 | 180–180 | 0 |
| Target verifications | 36 | verification | 3 | 36–36 | 0 |
| Accepted draft | 87 | token | 3 | 87–87 | 0 |
| Rejected draft | 93 | token | 3 | 93–93 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 36 | token | 3 | 36–36 | 0 |
| Per-sample mean accepted prefix | 2.41667 | token | 3 | 2.41667–2.41667 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 1.48043 | s | 3 | 1.46545–1.53546 | 0.0149748 |
| Verification phase | 11.443 | s | 3 | 11.4118–11.5292 | 0.0311844 |
| Speculative commit phase | 4.95095 | s | 3 | 4.95048–4.95519 | 0.000471478 |

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
| reasoning | high |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **reasoning.first.server**: server turn start to first source-classified reasoning token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **reasoning.first.client**: client dispatch including connect to first nonempty reasoning fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **reasoning.phase-rate**: source-classified reasoning tokens / server phase from prefill completion to reasoning boundary or decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-reasoning-high-chat-06/case-0/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Native protocol v24 candidate characterization, context4096/chunk512/explicit greedy; not the historical operator chunk64 configuration or chat terminal rendering.
- Same chat.short workload across reasoning modes; maximum keeps the source-authored longer instruction. Different policy/input populations are separate experiments, never averaged.
- Three completed capacity-limited 256-token turns. Reasoning terminator naturally reached; complete final answer and exact last-reasoning-to-first-final transition latency NOT MEASURED.
- Source/build/executable retained; built dirty-source delta unavailable in version JSON, so observer source capture is not substituted into target build provenance.
- Generated-token identity includes execution/state lineage, not just token IDs. Equal ordered fragment manifests establish published-content agreement only.
- Tiny prompt prefill is not a 2K/8K throughput gate. Channel phase rates are not sustained post-first-token channel rates.
- No independent admitted full-logit tolerance, original higher-precision quality comparison, release claim or 20/700 Task exit.
