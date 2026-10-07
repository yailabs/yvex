<!-- docs:metadata
title: "Installed DeepSeek native coding control, protocol 25"
id: yvex.evaluation.qualification.deepseek-installed-native-coding-21
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-native-coding-21.json
-->

# Installed DeepSeek native coding control, protocol 25

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-native-coding-21.json)

Target identity: `c915a176ebe6d318560f936ba2fc7aa340b970954a31ba5bfbfca249035c3e50`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 1.24194 | s | 3 | 1.20293–1.31516 | 0.0390057 |
| prefill.wall / coding.hash-table/turn-0 | 3.11563 | s | 3 | 3.10114–3.15381 | 0.0144886 |
| ttft.server / coding.hash-table/turn-0 | 4.53241 | s | 3 | 4.52537–4.56585 | 0.00704182 |
| ttft.client-visible / coding.hash-table/turn-0 | 5.77436 | s | 3 | 5.7283–5.88077 | 0.0460607 |
| request.client-complete / coding.hash-table/turn-0 | 69.1483 | s | 3 | 69.0104–69.1794 | 0.0311517 |
| final.first.server / coding.hash-table/turn-0 | 4.53241 | s | 3 | 4.52537–4.56585 | 0.00704182 |
| final.first.client / coding.hash-table/turn-0 | 5.77436 | s | 3 | 5.7283–5.88077 | 0.0460607 |
| prefill.uncached / coding.hash-table/turn-0 | 9.94983 | token/s | 3 | 9.82938–9.99632 | 0.0464857 |
| decode.post-first.committed / coding.hash-table/turn-0 | 4.0286 | token/s | 3 | 4.02382–4.02965 | 0.00104632 |
| final.phase-rate / coding.hash-table/turn-0 | 3.95616 | token/s | 3 | 3.95039–3.9573 | 0.00114522 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.hash-table/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 31 | token | 3 | 31–31 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 31 | token | 3 | 31–31 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 46 | cycle | 3 | 46–46 | 0 |
| Draft forwards | 46 | forward | 3 | 46–46 | 0 |
| Proposed | 230 | token | 3 | 230–230 | 0 |
| Selected verification | 227 | token | 3 | 227–227 | 0 |
| Target verifications | 46 | verification | 3 | 46–46 | 0 |
| Accepted draft | 164 | token | 3 | 164–164 | 0 |
| Rejected draft | 63 | token | 3 | 63–63 | 0 |
| Discarded draft | 3 | token | 3 | 3–3 | 0 |
| Correction/bonus | 46 | token | 3 | 46–46 | 0 |
| Per-sample mean accepted prefix | 3.56522 | token | 3 | 3.56522–3.56522 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 3.82947 | s | 3 | 3.82644–3.83678 | 0.00303228 |
| Verification phase | 41.2688 | s | 3 | 41.2378–41.2942 | 0.0253837 |
| Speculative commit phase | 19.0269 | s | 3 | 19.0002–19.0711 | 0.0267348 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not qualified by timing capture;  |
| checkpoint-reference | UNQUALIFIED | Not qualified by timing capture;  |
| representation-quality | UNQUALIFIED | Not qualified by timing capture;  |
| backend-execution | UNQUALIFIED | Not qualified by timing capture;  |
| deployment-performance | CHARACTERIZED | Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only;  |
| product-path | CHARACTERIZED | Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only;  |

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
| specialization | 6c3e33a0ea61e9a371dc551d595b5a68e6f28e3059cde0ee49fc1370939c8350 |
| source_commit | a46ddf5ee8c8b0fc4938a53f02fc372d9a00d116 |
| source_tree | 31387594c05c11e98a48a67c6fb92db19c60be7d |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | a7f245b5c014c4b3c1da3b591c164bd73f733945e130a8d2ce9fac92410805e7 |
| executable | c7cfe0e0684bad13016154097ad14bfa08f30914b445c086276b912a048947ba |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | e0a9e6d5569490cfefa1bd2304f5c881ae4473354f165ad8df3d67b0a9163985 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | greedy |
| product_path | product-native/native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/installed-native-hash-table-v21/observations.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing capture does not qualify model quality, upstream conformance or another quantization.
- Suite applicability is joined to the exact artifact/binding catalog relationship, not inferred from a model name.
- Producer and adapter source/build identities remain distinct. Unknown hardware/kernel dimensions refuse full comparison.
- Native measurements are neither HTTP timing nor terminal rendering; client TTFT includes dispatch/admission.
- Different published histories remain separate input groups. Outputs shorter than 33 committed tokens have no sustained rate.
- First publication and unreached reasoning-to-final transition remain NOT MEASURED. No load timing is inferred.
- Sampled clear intervals are not uninterrupted hardware reservations; no global model-throughput or release claim.
