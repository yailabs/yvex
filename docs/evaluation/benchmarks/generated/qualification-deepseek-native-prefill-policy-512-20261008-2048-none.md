<!-- docs:metadata
title: "GB10 Rust/native chunk 512 / 2k / DSpark / none"
id: yvex.evaluation.qualification.deepseek-native-prefill-policy-512-20261008-2048-none
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-prefill-policy-512-20261008-2048-none.json
-->

# GB10 Rust/native chunk 512 / 2k / DSpark / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-prefill-policy-512-20261008-2048-none.json)

Target identity: `fc8e7b70a646efe7b0b814e33304e35c1a11466d700e8e1f304067e9e4ba1d66`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.792677 | s | 3 | 0.788315–0.8052 | 0.00436122 |
| request.client-complete / prefill.promessi-2048/turn-0 | 24.8834 | s | 3 | 24.7788–24.9358 | 0.052423 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 23.0692 | s | 3 | 22.9574–23.0933 | 0.0241427 |
| ttft.server / prefill.promessi-2048/turn-0 | 22.2765 | s | 3 | 22.1522–22.305 | 0.0285174 |
| final.first.server / prefill.promessi-2048/turn-0 | 22.2765 | s | 3 | 22.1522–22.305 | 0.0285174 |
| final.first.client / prefill.promessi-2048/turn-0 | 23.0692 | s | 3 | 22.9574–23.0933 | 0.0241427 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 6.9226 | token/s | 3 | 6.51096–6.97918 | 0.0565746 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 94.4269 | token/s | 3 | 93.9572–94.5465 | 0.119589 |
| prefill.wall / prefill.promessi-2048/turn-0 | 21.6887 | s | 3 | 21.6613–21.7972 | 0.0274335 |

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
| Draft phase | 0.183765 | s | 3 | 0.180342–0.278311 | 0.00342341 |
| Verification phase | 1.41337 | s | 3 | 1.39184–1.43463 | 0.0212673 |
| Speculative commit phase | 0.667044 | s | 3 | 0.666834–0.69453 | 0.000209993 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-complete / prefill.promessi-2048/turn-0 | 7.24603158 | s | Single post-cache load-to-ready observation; cache uncontrolled, not a cold-load or repeated load benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/load.json |
| process-rss-peak / prefill.promessi-2048/turn-0 | 9.71280753e+10 | byte | Process-lifetime RSS peak across the three requests; includes file-backed weights, not separately additive GPU memory; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/cleanup.json |
| workspace-peak / prefill.promessi-2048/turn-0 | NOT MEASURED | byte | Typed workspace peak when available; unprojected ownership stays unavailable, not zero; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/cleanup.json |
| artifact-mapping-resident / prefill.promessi-2048/turn-0 | 9.50502113e+10 | byte | OS smaps artifact-mapping RSS at the post-request boundary; not an additional allocation or independent device copy; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/memory-after.json |

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
| source_commit | 299598ae0370efbad588bd0eec245a29b294fd6f |
| source_tree | dd5877160821adfa6042bf38018c3b900b54b610 |
| source_delta | 314aca7e1ce357f75db1df6a00bea1d5a7c2741e68f5c9dc8d53ed09ec160882 |
| build | 80f8c939453b22c082617ee1ebf68ff4e2dfd4e3d1a17b021a99a1fdde14cd30 |
| executable | ecc0dc8dc1718e715aac41407edd521f7a9529f0c10409838f2ad977bc251600 |
| backend | cuda |
| backend_implementation | backend.cuda@299598ae0370efbad588bd0eec245a29b294fd6f+314aca7e1ce357f75db1df6a00bea1d5a7c2741e68f5c9dc8d53ed09ec160882 |
| kernel_bundle | bc1d055de4abf0fc2cd7427c10c43eae39ebde5cead495062ca144fc8ca717bf |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 69410b521ea02dbf69c60d44b2a5d624b8e6c76412aba6ccf9f10abf7fc82f93 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":1.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-2k-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Existing native product defaults are greedy (stochastic=0, temperature=1), not stochastic HTTP defaults.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- Changed configured chunk is an explicit product-policy experiment, not a kernel or HTTP speedup. Native bundle identities differ between independently compiled builds and remain separately authenticated.
- Temperature/power/clock variation is not continuously observed; boundary/resource witness is not a hardware reservation.
