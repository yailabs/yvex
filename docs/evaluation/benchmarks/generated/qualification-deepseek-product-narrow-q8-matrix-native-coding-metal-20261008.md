<!-- docs:metadata
title: "GB10 Rust/native product coding.metal / none"
id: yvex.evaluation.qualification.deepseek-product-narrow-q8-matrix-native-coding-metal-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-product-narrow-q8-matrix-native-coding-metal-20261008.json
-->

# GB10 Rust/native product coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-product-narrow-q8-matrix-native-coding-metal-20261008.json)

Target identity: `a8e8cc60a414fd2e4d99af264e95f8dc0bdfb87baaba8e7848b70756e374756c`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.442411 | s | 3 | 0.423903–0.449523 | 0.00711269 |
| request.client-complete / coding.metal/turn-0 | 30.2096 | s | 3 | 30.2044–30.6042 | 0.00524537 |
| ttft.client-visible / coding.metal/turn-0 | 2.40438 | s | 3 | 2.40267–2.53299 | 0.00171094 |
| ttft.server / coding.metal/turn-0 | 1.96196 | s | 3 | 1.95314–2.10906 | 0.00881717 |
| final.first.server / coding.metal/turn-0 | 1.96196 | s | 3 | 1.95314–2.10906 | 0.00881717 |
| final.first.client / coding.metal/turn-0 | 2.40438 | s | 3 | 2.40267–2.53299 | 0.00171094 |
| final.phase-rate / coding.metal/turn-0 | 9.05 | token/s | 3 | 8.92137–9.05086 | 0.000862099 |
| decode.post-first.committed / coding.metal/turn-0 | 9.17095 | token/s | 3 | 9.0846–9.17324 | 0.00229656 |
| prefill.uncached / coding.metal/turn-0 | 35.9223 | token/s | 3 | 35.7421–36.0332 | 0.110831 |
| prefill.wall / coding.metal/turn-0 | 1.47541 | s | 3 | 1.47087–1.48285 | 0.00453805 |

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
| Draft phase | 2.43916 | s | 3 | 2.43272–2.54601 | 0.00644385 |
| Verification phase | 17.3202 | s | 3 | 17.2961–17.5546 | 0.024114 |
| Speculative commit phase | 8.00358 | s | 3 | 7.96802–8.04721 | 0.0355575 |

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
| source_commit | 46d5476930902fe2553bcbf8132636cad607bcd1 |
| source_tree | b27de27964233f1f475ae4b11bb045cc85753e65 |
| source_delta | 1bc378817ac310e08a905c9c4b0a0f48be170b169a6fa4cccc43ce7030bada5b |
| build | bb1812ece26f0d286da45bc6d31e0232ee10d0bd8828bd8001a0684141c29078 |
| executable | dc568abf2ddf403f6a6697ed869018ee017f154afe2e5a57b7d80bdc164dcdc9 |
| backend | cuda |
| backend_implementation | backend.cuda@46d5476930902fe2553bcbf8132636cad607bcd1+1bc378817ac310e08a905c9c4b0a0f48be170b169a6fa4cccc43ce7030bada5b |
| kernel_bundle | 8a83d53f5d106266ee11032d4967f2f3523e2528ea70f98759cc9e5c34ba16ab |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 2fe545fcfe14bfe184a6f879c1dc878de59af9da04b2aefd757c803946264f36 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":1.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-narrow-q8-matrix-native-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Existing native product defaults are greedy (stochastic=0, temperature=1), not stochastic HTTP defaults.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
