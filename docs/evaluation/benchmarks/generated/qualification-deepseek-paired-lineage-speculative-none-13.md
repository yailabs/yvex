<!-- docs:metadata
title: "DeepSeek representative native corpus / speculative / none"
id: yvex.evaluation.qualification.deepseek-paired-lineage-speculative-none-13
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-paired-lineage-speculative-none-13.json
-->

# DeepSeek representative native corpus / speculative / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-paired-lineage-speculative-none-13.json)

Target identity: `fbd53bd45668f7a813933085382800bc33a1b8b4fec802a3befa077374f31c66`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 1.41118 | s | 3 | 1.40133–1.41607 | 0.00489363 |
| request.client-complete / coding.metal/turn-0 | 34.6353 | s | 3 | 34.6176–34.7157 | 0.0176282 |
| ttft.client-visible / coding.metal/turn-0 | 3.57418 | s | 3 | 3.56185–3.58151 | 0.00732428 |
| ttft.server / coding.metal/turn-0 | 2.16299 | s | 3 | 2.1605–2.16537 | 0.00237955 |
| final.first.server / coding.metal/turn-0 | 2.16299 | s | 3 | 2.1605–2.16537 | 0.00237955 |
| final.first.client / coding.metal/turn-0 | 3.57418 | s | 3 | 3.56185–3.58151 | 0.00732428 |
| final.phase-rate / coding.metal/turn-0 | 8.10236 | token/s | 3 | 8.08205–8.11046 | 0.00809622 |
| decode.post-first.committed / coding.metal/turn-0 | 8.20685 | token/s | 3 | 8.1908–8.21481 | 0.00796102 |
| prefill.uncached / coding.metal/turn-0 | 32.3921 | token/s | 3 | 32.3147–32.6642 | 0.0774089 |
| prefill.wall / coding.metal/turn-0 | 1.6362 | s | 3 | 1.62257–1.64012 | 0.00391947 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

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
| Draft phase | 2.53374 | s | 3 | 2.52295–2.53686 | 0.00311932 |
| Verification phase | 19.6657 | s | 3 | 19.5899–19.6848 | 0.0190473 |
| Speculative commit phase | 8.86996 | s | 3 | 8.84688–8.89303 | 0.0230725 |

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
| tokenizer_conversation | 4d489a7e6340ca30f5cd6e72af004347eb246e2b9521eaf6f7c6ae56b5f995dd |
| transformation_ir | f1fca7b4ec04d1b0de2a0f0707b3f78c5600e9a6486a83c6fc9f3a4bd70f88e8 |
| physical_policy | 59dd7bdabf6b81989dfa14e0f70692805a8f02a473afcc040a3e55083f48dda0 |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | 93ab856045bbaf0f582d5389aed04d15157f3a42fc1f3046e442fb9890589303 |
| build | 5c6c87fe3124f21afda23cadd29f87270a787e26806b02a8d0752d382d1b7d68 |
| executable | 1a6da4bf70b0e866b2a5a03f40585366ed1b1a913526fc78f1cd5d51c94f53f8 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 8f04397d4407ed2aab45bc0e23d0dc73404913e8473523e528f6b79adabd2532 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/case-0/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Native protocol v24, not HTTP or terminal rendering. Isolated resident host, context4096/chunk512 and explicit greedy; not the historical operator chunk64 configuration.
- Individual workload rows are not averaged. Actual committed counts and early EOS remain visible; a short completion does not establish sustained decode.
- Only the declared reasoning and execution strategy are measured; other modes require separate target-bound evidence. The name of a math case does not establish reasoning qualification.
- Tiny-input prefill does not establish 2K/8K throughput. First committed-fragment publication and exact reasoning-to-final transition timing remain unavailable.
- Built-source delta comes from the executable projection; observer snapshot uses the separate QA source-capture algorithm.
- No independently admitted full-logit tolerance or original higher-precision quality gate; not release performance or the20/700 Task exit.
- External process observation accompanies timed samples; sampled clear lists do not exclude activity between probes. Probe overhead/cadence remain in the witness.
