<!-- docs:metadata
title: "DeepSeek coding / HTTP / speculative / none / lineage witness"
id: yvex.evaluation.qualification.deepseek-paired-lineage-http-none-13
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-paired-lineage-http-none-13.json
-->

# DeepSeek coding / HTTP / speculative / none / lineage witness

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-paired-lineage-http-none-13.json)

Target identity: `b05da84a2cf1a61a341f482e5ed69b4aa4a2c3bb78e0c16d8dbf5b25fdf2cfaa`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| request.client-complete / coding.metal/turn-0 | 35.3113 | s | 3 | 35.1815–35.3602 | 0.048979 |
| ttft.client-visible / coding.metal/turn-0 | 3.64664 | s | 3 | 3.57383–3.81971 | 0.0728146 |
| ttft.server / coding.metal/turn-0 | 2.17352 | s | 3 | 2.15679–2.41382 | 0.0167299 |
| prefill.wall / coding.metal/turn-0 | 1.63561 | s | 3 | 1.61659–1.6485 | 0.0128914 |
| prefill.uncached / coding.metal/turn-0 | 32.4039 | token/s | 3 | 32.1505–32.7851 | 0.253401 |
| decode.post-first.committed / coding.metal/turn-0 | 8.23821 | token/s | 3 | 8.23702–8.28027 | 0.00119368 |

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
| Draft forwards | NOT MEASURED | forward | — | — | — |
| Proposed | 300 | token | 3 | 300–300 | 0 |
| Selected verification | 298 | token | 3 | 298–298 | 0 |
| Target verifications | 60 | verification | 3 | 60–60 | 0 |
| Accepted draft | 136 | token | 3 | 136–136 | 0 |
| Rejected draft | 162 | token | 3 | 162–162 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | NOT MEASURED | token | — | — | — |
| Per-sample mean accepted prefix | NOT MEASURED | token | — | — | — |
| Per-sample maximum accepted prefix | NOT MEASURED | token | — | — | — |
| Draft phase | NOT MEASURED | s | — | — | — |
| Verification phase | NOT MEASURED | s | — | — | — |
| Speculative commit phase | NOT MEASURED | s | — | — | — |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | CHARACTERIZED | Three fresh coding requests over HTTP; exact same resident binary/profile as the separately recorded native lane;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three fresh coding requests over HTTP; exact same resident binary/profile as the separately recorded native lane;  |
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
| sampling | {"min_p":0,"seed":null,"seed_origin":"not-applicable","stochastic":false,"temperature":0,"top_k":0,"top_p":1,"typical_p":1} |
| product_path | http-openai |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **request.client-complete**: client dispatch including connect through terminal response. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: HTTP client and correlated server phases; not native chat or representation quality. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-lineage-speculative-none-13/http-case-0/observations.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Same binary/model/context/chunk/declared greedy sampling as the paired native record; transports remain separate targets.
- Logical workload matches, but exact rendered input-token identity is not projected by HTTP; do not silently substitute the native input digest.
- HTTP and native sampling receipts use their respective producer projections. No transport-only causal latency gain is inferred.
- Only 53 new input positions and 256 committed output tokens per sample: not the 2K/8K prefill gate or universal sustained decode.
- Independent same-weight captures are separate reference evidence, not an admitted higher-precision quality or continuation-agreement gate.
- Kernel/toolkit/backend identity gaps remain explicit. All planes are independent; this is CHARACTERIZED, not qualified or release performance.
- No committed-fragment publication timestamp, cold-load comparison or uninterrupted hardware reservation is inferred.
- Sampled process-list observation cadence and probe time remain in each raw witness.
