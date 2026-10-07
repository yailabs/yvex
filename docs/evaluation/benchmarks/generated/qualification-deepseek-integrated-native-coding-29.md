<!-- docs:metadata
title: "Integrated computational candidate: native coding, product chunk64 and DSpark"
id: yvex.evaluation.qualification.deepseek-integrated-native-coding-29
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-integrated-native-coding-29.json
-->

# Integrated computational candidate: native coding, product chunk64 and DSpark

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-integrated-native-coding-29.json)

Target identity: `417305bdd2bbdf7a1da09f3e6a7ed75ebe6c6714de69577d325850de4093bd48`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.525884 | s | 3 | 0.522069–0.549001 | 0.00381523 |
| prefill.wall / coding.hash-table/turn-0 | 1.17782 | s | 3 | 1.17255–1.18731 | 0.00527309 |
| ttft.server / coding.hash-table/turn-0 | 1.69871 | s | 3 | 1.69841–1.71011 | 0.000305889 |
| ttft.client-visible / coding.hash-table/turn-0 | 2.23602 | s | 3 | 2.22089–2.24751 | 0.0114876 |
| request.client-complete / coding.hash-table/turn-0 | 24.9059 | s | 3 | 24.8678–24.9073 | 0.0013518 |
| final.first.server / coding.hash-table/turn-0 | 1.69871 | s | 3 | 1.69841–1.71011 | 0.000305889 |
| final.first.client / coding.hash-table/turn-0 | 2.23602 | s | 3 | 2.22089–2.24751 | 0.0114876 |
| prefill.uncached / coding.hash-table/turn-0 | 26.3198 | token/s | 3 | 26.1094–26.4382 | 0.118363 |
| decode.post-first.committed / coding.hash-table/turn-0 | 11.2491 | token/s | 3 | 11.241–11.2739 | 0.00815312 |
| final.phase-rate / coding.hash-table/turn-0 | 11.0387 | token/s | 3 | 11.0318–11.0611 | 0.0069181 |

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
| Draft phase | 1.88491 | s | 3 | 1.87271–1.88632 | 0.00140348 |
| Verification phase | 14.5238 | s | 3 | 14.4934–14.5441 | 0.0202485 |
| Speculative commit phase | 6.35281 | s | 3 | 6.3502–6.36007 | 0.00260925 |

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
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | e771219f919bdb8d91a7a47702d5e252edf7fa13 |
| source_tree | fc34ea9ab1e2121ae2911296ecd857058ce77202 |
| source_delta | c4031f5a1b5fe42ccc9803a91a23ad608c27bff82dc1d5b310f78197d9dcadf0 |
| build | e895734b9da9138102c74721c6f6bad1480fc10fc7bcf96c31590c1f05345e9a |
| executable | 9c42bc28f38993045bec4c5466e6c8e39c2581229e8d8a726d2e993278026377 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | ab95e7597d8d0b65174c5b8241d0a66f6bb1fc1d6476ee961e04e2c52ab765cb |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Closed product-native capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-native-product-coding-v29/coding/observations.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing capture does not qualify model quality, upstream conformance or another quantization.
- Suite applicability is joined to the exact artifact/binding catalog relationship, not inferred from a model name.
- Producer and adapter source/build identities remain distinct. Unknown hardware/kernel dimensions refuse full comparison.
- Native measurements are neither HTTP timing nor terminal rendering; client TTFT includes dispatch/admission.
- Different published histories remain separate input groups. Outputs shorter than 33 committed tokens have no sustained rate.
- First publication and unreached reasoning-to-final transition remain NOT MEASURED. No load timing is inferred.
- Sampled clear intervals are not uninterrupted hardware reservations; no global model-throughput or release claim.
