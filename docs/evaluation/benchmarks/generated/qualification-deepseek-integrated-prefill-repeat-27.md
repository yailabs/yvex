<!-- docs:metadata
title: "Frozen candidate repeated 8192-position uncached prefill; internal failure regression control"
id: yvex.evaluation.qualification.deepseek-integrated-prefill-repeat-27
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-integrated-prefill-repeat-27.json
-->

# Frozen candidate repeated 8192-position uncached prefill; internal failure regression control

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-integrated-prefill-repeat-27.json)

Target identity: `ee7478d3ed0b99827c770a0726791b2fd89c2c8d78fcb45645e104b3182a44bb`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 1.01854 | s | 2 | 0.984738–1.05234 | 0.0338031 |
| prefill.wall / prefill.promessi-8192/turn-0 | 114.681 | s | 2 | 114.614–114.749 | 0.0674609 |
| ttft.server / prefill.promessi-8192/turn-0 | 114.83 | s | 2 | 114.763–114.897 | 0.0668634 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 115.849 | s | 2 | 115.748–115.949 | 0.100605 |
| request.client-complete / prefill.promessi-8192/turn-0 | 117.936 | s | 2 | 117.837–118.034 | 0.0982445 |
| final.first.server / prefill.promessi-8192/turn-0 | 114.83 | s | 2 | 114.763–114.897 | 0.0668634 |
| final.first.client / prefill.promessi-8192/turn-0 | 115.849 | s | 2 | 115.748–115.949 | 0.100605 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 71.4327 | token/s | 2 | 71.3907–71.4747 | 0.04202 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 7.16849 | token/s | 2 | 7.15891–7.17808 | 0.00958533 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-8192/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 8192 | token | 2 | 8192–8192 | 0 |
| Reused prefix | 0 | token | 2 | 0–0 | 0 |
| New prefill | 8192 | token | 2 | 8192–8192 | 0 |
| Committed output | 16 | token | 2 | 16–16 | 0 |
| Reasoning | 0 | token | 2 | 0–0 | 0 |
| Final content | 16 | token | 2 | 16–16 | 0 |
| Draft cycles | 0 | cycle | 2 | 0–0 | 0 |
| Draft forwards | 0 | forward | 2 | 0–0 | 0 |
| Proposed | 0 | token | 2 | 0–0 | 0 |
| Selected verification | 0 | token | 2 | 0–0 | 0 |
| Target verifications | 0 | verification | 2 | 0–0 | 0 |
| Accepted draft | 0 | token | 2 | 0–0 | 0 |
| Rejected draft | 0 | token | 2 | 0–0 | 0 |
| Discarded draft | 0 | token | 2 | 0–0 | 0 |
| Correction/bonus | 0 | token | 2 | 0–0 | 0 |
| Per-sample mean accepted prefix | 0 | token | 2 | 0–0 | 0 |
| Per-sample maximum accepted prefix | 0 | token | 2 | 0–0 | 0 |
| Draft phase | 0 | s | 2 | 0–0 | 0 |
| Verification phase | 0 | s | 2 | 0–0 | 0 |
| Speculative commit phase | 0 | s | 2 | 0–0 | 0 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not qualified by timing capture;  |
| checkpoint-reference | UNQUALIFIED | Not qualified by timing capture;  |
| representation-quality | UNQUALIFIED | Not qualified by timing capture;  |
| backend-execution | UNQUALIFIED | Not qualified by timing capture;  |
| deployment-performance | CHARACTERIZED | Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only;  |
| product-path | CHARACTERIZED | Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only;  |

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
| source_commit | c57d333bb3a3f63d9c0d720c8016455cdeb7da41 |
| source_tree | b525c7379420a0530606e02e9bba5982099bd4e7 |
| source_delta | 776e0d141f751913889198e2a2ad4a44516b559f3e1c261274bd74f9c166e914 |
| build | 971f7e639da861d00b1b84bbbb8821fcc8c531f2326d97a78b0420b54de33dab |
| executable | f5c2902d747a521990851808f725ec791598a8c5f5ea2bc4d750c673d003137d |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | d5189e76de9ac4c354e448164445971f04fde8a006e4717dbc20b8ee314fdee9 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy |
| product_path | controlled-engine/native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Closed controlled-engine capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only. Session: fresh; warm/cold: resident-engine; first-request-and-repeats-separated; first 1 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/integrated-candidate-prefill-repeat-v27/coding/observations.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing capture does not qualify model quality, upstream conformance or another quantization.
- Suite applicability is joined to the exact artifact/binding catalog relationship, not inferred from a model name.
- Producer and adapter source/build identities remain distinct. Unknown hardware/kernel dimensions refuse full comparison.
- Native measurements are neither HTTP timing nor terminal rendering; client TTFT includes dispatch/admission.
- Different published histories remain separate input groups. Outputs shorter than 33 committed tokens have no sustained rate.
- First publication and unreached reasoning-to-final transition remain NOT MEASURED. No load timing is inferred.
- Sampled clear intervals are not uninterrupted hardware reservations; no global model-throughput or release claim.
