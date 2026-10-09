<!-- docs:metadata
title: "REJECTED execution candidate / GB10 controlled configuration / Rust-native coding.hash-table / none"
id: yvex.evaluation.qualification.deepseek-computational5-rejected-scaled-q8-speculative-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-rejected-scaled-q8-speculative-20261009.json
-->

# REJECTED execution candidate / GB10 controlled configuration / Rust-native coding.hash-table / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-rejected-scaled-q8-speculative-20261009.json)

Target identity: `560ba6ba16c5fd3344e5803c673c55e178d1e30a19a6f3d72237173725d79059`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.57598 | s | 3 | 0.570154–0.607742 | 0.00582632 |
| request.client-complete / coding.hash-table/turn-0 | 20.6112 | s | 3 | 20.6028–20.986 | 0.00846338 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.83889 | s | 3 | 1.78499–1.92408 | 0.0538955 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.453156 | s | 3 | 0.444809–0.465595 | 0.00834714 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0738262 | s | 3 | 0.073583–0.0747502 | 0.000243191 |
| ttft.server / coding.hash-table/turn-0 | 1.23112 | s | 3 | 1.20891–1.35393 | 0.0222142 |
| final.first.server / coding.hash-table/turn-0 | 1.23112 | s | 3 | 1.20891–1.35393 | 0.0222142 |
| final.first.client / coding.hash-table/turn-0 | 1.83889 | s | 3 | 1.78499–1.92408 | 0.0538955 |
| final.phase-rate / coding.hash-table/turn-0 | 13.2909 | token/s | 3 | 13.0511–13.3272 | 0.0363366 |
| decode.post-first.committed / coding.hash-table/turn-0 | 13.5454 | token/s | 3 | 13.378–13.5903 | 0.0449405 |
| prefill.uncached / coding.hash-table/turn-0 | 39.4617 | token/s | 3 | 38.771–40.0995 | 0.637789 |
| prefill.wall / coding.hash-table/turn-0 | 0.785571 | s | 3 | 0.773076–0.799567 | 0.0124946 |

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
| Draft phase | 1.55791 | s | 3 | 1.54208–1.66082 | 0.0158286 |
| Verification phase | 11.8253 | s | 3 | 11.7554–11.9413 | 0.069841 |
| Speculative commit phase | 5.54537 | s | 3 | 5.54184–5.66338 | 0.00353187 |

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
| source_commit | 25b981808348440f37cde5913d4d804c4940e5a7 |
| source_tree | a6510628c9b45227f5bd4624a1f7acd0b9c4674c |
| source_delta | 3ec457a2b8a38e24dff123d8e9cdb1e9119ecc5ada806c8f50b9f9f0e11f185e |
| build | 4c840b4150ce39aa9ac1dd4b63dcff04ba129df01300ab6d6328e8ac98ae12dc |
| executable | fca775509e1a8fea1481bbcb2982f21540487c1b065c4eb0689a39ce2c7aa4b8 |
| backend | cuda |
| backend_implementation | backend.cuda@25b981808348440f37cde5913d4d804c4940e5a7+3ec457a2b8a38e24dff123d8e9cdb1e9119ecc5ada806c8f50b9f9f0e11f185e |
| kernel_bundle | cad9ad084c826d0c9a4e629d3e39738e8d0f0cd0718c5f34f5b9923744cd1d14 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663165952 |
| runtime_configuration | 6add86bbd10885203aab28e939a69890b69e5227e83abfc3a375fd35572fe02a |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/scaled-q8-speculative-coding-01/native/events.jsonl.

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
- Lossless Q8 activation preparation earns no speculative benefit; exact acceptance population retained. Not retained.
