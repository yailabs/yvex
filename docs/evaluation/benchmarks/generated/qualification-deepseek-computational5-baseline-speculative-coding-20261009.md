<!-- docs:metadata
title: "GB10 controlled configuration / Rust-native coding.hash-table / none"
id: yvex.evaluation.qualification.deepseek-computational5-baseline-speculative-coding-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-baseline-speculative-coding-20261009.json
-->

# GB10 controlled configuration / Rust-native coding.hash-table / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-baseline-speculative-coding-20261009.json)

Target identity: `9ecd175914236d0d8ead8117d227db1166a86e23ce34e312d907e7e0f1401620`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.571099 | s | 3 | 0.548416–0.580431 | 0.00933235 |
| request.client-complete / coding.hash-table/turn-0 | 20.3592 | s | 3 | 20.3276–20.6842 | 0.0315893 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.79752 | s | 3 | 1.74245–1.89856 | 0.0550696 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.43918 | s | 3 | 0.436577–0.446948 | 0.0026032 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0728809 | s | 3 | 0.0727888–0.073667 | 9.20917e-05 |
| ttft.server / coding.hash-table/turn-0 | 1.22641 | s | 3 | 1.19401–1.31813 | 0.0324003 |
| final.first.server / coding.hash-table/turn-0 | 1.22641 | s | 3 | 1.19401–1.31813 | 0.0324003 |
| final.first.client / coding.hash-table/turn-0 | 1.79752 | s | 3 | 1.74245–1.89856 | 0.0550696 |
| final.phase-rate / coding.hash-table/turn-0 | 13.4659 | token/s | 3 | 13.2431–13.472 | 0.00611881 |
| decode.post-first.committed / coding.hash-table/turn-0 | 13.7211 | token/s | 3 | 13.5747–13.7384 | 0.0173673 |
| prefill.uncached / coding.hash-table/turn-0 | 40.1608 | token/s | 3 | 39.4893–40.3926 | 0.231803 |
| prefill.wall / coding.hash-table/turn-0 | 0.771897 | s | 3 | 0.767467–0.785023 | 0.00442972 |

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
| Draft phase | 1.53397 | s | 3 | 1.52476–1.63867 | 0.00920422 |
| Verification phase | 11.6753 | s | 3 | 11.6671–11.8187 | 0.00821872 |
| Speculative commit phase | 5.46258 | s | 3 | 5.45363–5.53126 | 0.00895621 |

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
| source_commit | 337e7e73057abd8a784a67eea58ad8a6f079869d |
| source_tree | 14b7fea18275a542739d4d2afaa38f6fd720d407 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | a72b5d98f4361eb97eb051d761a7133268d1f97da66d7543d9fecc7046a0b857 |
| executable | eb108d22b68674f92a5430e246723e4e7f0505ae4d0eef01beb590845a8fba7d |
| backend | cuda |
| backend_implementation | backend.cuda@337e7e73057abd8a784a67eea58ad8a6f079869d+e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| kernel_bundle | c4157a8b2964602100bcb1bb4f895b044ad8ca221b4098b778c67d176dbf2468 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663165952 |
| runtime_configuration | 378c51914be600770757f9eb71c2c91db774c2913b2ddc83674e36c454bbf0f0 |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-configuration/native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-speculative-coding-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Controlled engine configuration exercised through the Rust/native client; not unchanged operator defaults. Context, strategy, prefill geometry and sampling are explicit experimental axes.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Sampling selection is greedy; resolved facts in receipt; exact temperature/stochastic fields are retained per observation. Native and HTTP defaults are not interchangeable.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
