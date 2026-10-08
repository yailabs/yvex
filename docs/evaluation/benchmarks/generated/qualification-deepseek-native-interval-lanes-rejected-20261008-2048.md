<!-- docs:metadata
title: "REJECTED execution candidate / GB10 Rust/native product prefill.promessi-2048 / none"
id: yvex.evaluation.qualification.deepseek-native-interval-lanes-rejected-20261008-2048
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-interval-lanes-rejected-20261008-2048.json
-->

# REJECTED execution candidate / GB10 Rust/native product prefill.promessi-2048 / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-interval-lanes-rejected-20261008-2048.json)

Target identity: `a8466fb54bfea3787c74cea474574e439bdf5fe7959e27d6f5b08ff90c23e892`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.788019 | s | 3 | 0.779698–0.831096 | 0.00832137 |
| request.client-complete / prefill.promessi-2048/turn-0 | 25.7775 | s | 3 | 25.7175–25.8904 | 0.0600782 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 23.7336 | s | 3 | 23.671–23.8198 | 0.0625975 |
| ttft.server / prefill.promessi-2048/turn-0 | 22.9455 | s | 3 | 22.84–23.0401 | 0.0946777 |
| final.first.server / prefill.promessi-2048/turn-0 | 22.9455 | s | 3 | 22.84–23.0401 | 0.0946777 |
| final.first.client / prefill.promessi-2048/turn-0 | 23.7336 | s | 3 | 23.671–23.8198 | 0.0625975 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 6.17349 | token/s | 3 | 5.85489–6.19489 | 0.0213962 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 91.5249 | token/s | 3 | 91.4069–91.8661 | 0.117952 |
| prefill.wall / prefill.promessi-2048/turn-0 | 22.3764 | s | 3 | 22.2933–22.4053 | 0.0288747 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-2048/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 2048 | token | 3 | 2048–2048 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 2048 | token | 3 | 2048–2048 | 0 |
| Committed output | 16 | token | 3 | 16–16 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 16 | token | 3 | 16–16 | 0 |
| Draft cycles | 5 | cycle | 3 | 5–5 | 0 |
| Draft forwards | 5 | forward | 3 | 5–5 | 0 |
| Proposed | 25 | token | 3 | 25–25 | 0 |
| Selected verification | 24 | token | 3 | 24–24 | 0 |
| Target verifications | 5 | verification | 3 | 5–5 | 0 |
| Accepted draft | 6 | token | 3 | 6–6 | 0 |
| Rejected draft | 18 | token | 3 | 18–18 | 0 |
| Discarded draft | 1 | token | 3 | 1–1 | 0 |
| Correction/bonus | 5 | token | 3 | 5–5 | 0 |
| Per-sample mean accepted prefix | 1.2 | token | 3 | 1.2–1.2 | 0 |
| Per-sample maximum accepted prefix | 4 | token | 3 | 4–4 | 0 |
| Draft phase | 0.269846 | s | 3 | 0.265485–0.35924 | 0.00436076 |
| Verification phase | 1.57809 | s | 3 | 1.57351–1.615 | 0.00458211 |
| Speculative commit phase | 0.691299 | s | 3 | 0.690966–0.707583 | 0.000332706 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Not independently qualified by this timing capture;  |
| checkpoint-reference | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |
| deployment-performance | CHARACTERIZED | Frozen Rust/native request path; not interactive terminal-render timing or HTTP;  |
| family-conformance | UNQUALIFIED | Not independently qualified by this timing capture;  |
| product-path | CHARACTERIZED | Frozen Rust/native request path; not interactive terminal-render timing or HTTP;  |
| representation-quality | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |

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
| source_commit | e167b04b5ee342a37cc5e9a249fa3c917223fe03 |
| source_tree | 5c0550e89e4a110064875ed1c325a4354dda49ee |
| source_delta | 5e042cc2e0440786dfc1166d98994ca3fb1349153a86e1750193c122ad4647cc |
| build | 86b253443df3334f0c8e029cece3cf32678f545d46d1fc399a598cd75aca9ddb |
| executable | ce430dd609bc92493bd6e35ce14f7fe8474a4c4463a587869f3652a431311605 |
| backend | cuda |
| backend_implementation | backend.cuda@e167b04b5ee342a37cc5e9a249fa3c917223fe03+5e042cc2e0440786dfc1166d98994ca3fb1349153a86e1750193c122ad4647cc |
| kernel_bundle | 956b9e36f15aa3106189345c0f1cc5911e7dada84a08059f753f5b7d0570dbfa |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 33c49875d1e2d16d106f6a284f0135219302e24d09c184762f648bb60e7b3253 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-lanes-candidate-2k-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Sampling selection is greedy; resolved facts in receipt; exact temperature/stochastic fields are retained per observation. Native and HTTP defaults are not interchangeable.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- Two independent interval accumulators preserve this continuation but regress complete coding and 2K prefill. Not retained.
