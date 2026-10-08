<!-- docs:metadata
title: "GB10 Rust/native chunk 512 / 8k / DSpark / none"
id: yvex.evaluation.qualification.deepseek-native-prefill-policy-512-20261008-8192-none
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-prefill-policy-512-20261008-8192-none.json
-->

# GB10 Rust/native chunk 512 / 8k / DSpark / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-prefill-policy-512-20261008-8192-none.json)

Target identity: `7cada69e4cf2c0eb253a2d3e010ce7f4b21e4b1a63f0c486119e7045b689b416`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.877425 | s | 3 | 0.856419–1.48644 | 0.0210065 |
| request.client-complete / prefill.promessi-8192/turn-0 | 103.093 | s | 3 | 102.962–104.391 | 0.130221 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 101.198 | s | 3 | 101.06–102.429 | 0.138021 |
| ttft.server / prefill.promessi-8192/turn-0 | 100.32 | s | 3 | 100.203–100.943 | 0.117001 |
| final.first.server / prefill.promessi-8192/turn-0 | 100.32 | s | 3 | 100.203–100.943 | 0.117001 |
| final.first.client / prefill.promessi-8192/turn-0 | 101.198 | s | 3 | 101.06–102.429 | 0.138021 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 6.60866 | token/s | 3 | 6.09396–6.65172 | 0.0430596 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 82.0786 | token/s | 3 | 81.6944–82.1814 | 0.102792 |
| prefill.wall / prefill.promessi-8192/turn-0 | 99.8068 | s | 3 | 99.6819–100.276 | 0.124838 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-8192/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 8192 | token | 3 | 8192–8192 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 8192 | token | 3 | 8192–8192 | 0 |
| Committed output | 16 | token | 3 | 16–16 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 16 | token | 3 | 16–16 | 0 |
| Draft cycles | 5 | cycle | 3 | 5–5 | 0 |
| Draft forwards | 5 | forward | 3 | 5–5 | 0 |
| Proposed | 25 | token | 3 | 25–25 | 0 |
| Selected verification | 23 | token | 3 | 23–23 | 0 |
| Target verifications | 5 | verification | 3 | 5–5 | 0 |
| Accepted draft | 6 | token | 3 | 6–6 | 0 |
| Rejected draft | 17 | token | 3 | 17–17 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | 5 | token | 3 | 5–5 | 0 |
| Per-sample mean accepted prefix | 1.2 | token | 3 | 1.2–1.2 | 0 |
| Per-sample maximum accepted prefix | 3 | token | 3 | 3–3 | 0 |
| Draft phase | 0.184905 | s | 3 | 0.183884–0.287358 | 0.00102101 |
| Verification phase | 1.46974 | s | 3 | 1.4577–1.50932 | 0.0120352 |
| Speculative commit phase | 0.716584 | s | 3 | 0.712113–0.775198 | 0.00447099 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-complete / prefill.promessi-8192/turn-0 | 7.02580201 | s | Single post-cache load-to-ready observation; cache uncontrolled, not a cold-load or repeated load benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/load.json |
| process-rss-peak / prefill.promessi-8192/turn-0 | 9.73830636e+10 | byte | Process-lifetime RSS peak across the three requests; includes file-backed weights, not separately additive GPU memory; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/cleanup.json |
| workspace-peak / prefill.promessi-8192/turn-0 | NOT MEASURED | byte | Typed workspace peak when available; unprojected ownership stays unavailable, not zero; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/cleanup.json |
| artifact-mapping-resident / prefill.promessi-8192/turn-0 | 9.50279291e+10 | byte | OS smaps artifact-mapping RSS at the post-request boundary; not an additional allocation or independent device copy; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/memory-after.json |

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
| runtime_configuration | 88341a79112d26f93914e573900a3df67632eb02af044cc8a65bef1d46b23846 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-candidate-8k-01/native/events.jsonl.

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
