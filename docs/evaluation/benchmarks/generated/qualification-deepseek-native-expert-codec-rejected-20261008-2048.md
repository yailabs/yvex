<!-- docs:metadata
title: "REJECTED execution candidate / GB10 Rust/native product prefill.promessi-2048 / none"
id: yvex.evaluation.qualification.deepseek-native-expert-codec-rejected-20261008-2048
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-expert-codec-rejected-20261008-2048.json
-->

# REJECTED execution candidate / GB10 Rust/native product prefill.promessi-2048 / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-expert-codec-rejected-20261008-2048.json)

Target identity: `72fb71a8f359cd9c3c46a025a80b21a00ff0a04d6a4da8e5162b9c89282b1d0e`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.806772 | s | 3 | 0.805423–0.821126 | 0.00134915 |
| request.client-complete / prefill.promessi-2048/turn-0 | 24.7727 | s | 3 | 24.7726–24.7844 | 4.1945e-05 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 22.9595 | s | 3 | 22.9458–22.9609 | 0.00149563 |
| ttft.server / prefill.promessi-2048/turn-0 | 22.1398 | s | 3 | 22.139–22.154 | 0.000806503 |
| final.first.server / prefill.promessi-2048/turn-0 | 22.1398 | s | 3 | 22.139–22.154 | 0.000806503 |
| final.first.client / prefill.promessi-2048/turn-0 | 22.9595 | s | 3 | 22.9458–22.9609 | 0.00149563 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 6.95661 | token/s | 3 | 6.55291–6.98822 | 0.0316033 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 94.5934 | token/s | 3 | 94.4796–95.1024 | 0.11381 |
| prefill.wall / prefill.promessi-2048/turn-0 | 21.6506 | s | 3 | 21.5347–21.6766 | 0.0260801 |

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
| Draft phase | 0.178441 | s | 3 | 0.178052–0.273905 | 0.000388081 |
| Verification phase | 1.40845 | s | 3 | 1.39449–1.43081 | 0.0139545 |
| Speculative commit phase | 0.666627 | s | 3 | 0.662543–0.686857 | 0.00408398 |

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
| source_delta | 485014bc4265e7916f4d21d16e1d720612392debae86035a25c23c1871da3cce |
| build | a28817927f600e868e5b68530c2541d825e2b641752d35d3e5f4657cb5891f9e |
| executable | 293092a40cc06646f658aea44e7e3847b217e9df68644328d3d78f3f6baaebdb |
| backend | cuda |
| backend_implementation | backend.cuda@e167b04b5ee342a37cc5e9a249fa3c917223fe03+485014bc4265e7916f4d21d16e1d720612392debae86035a25c23c1871da3cce |
| kernel_bundle | 66fd9d8f46a278f7258eb11cb1906458ab490c968d04cf73259c2919d861ae8d |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 88fa8e55936961bfff3f98b33522bc574d331315a4da044874ef824d1dc28049 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-2k-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Sampling selection is greedy; resolved facts in receipt; exact temperature/stochastic fields are retained per observation. Native and HTTP defaults are not interchangeable.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- Invocation-invariant expert codec specialization has no material complete-model improvement. Not retained.
