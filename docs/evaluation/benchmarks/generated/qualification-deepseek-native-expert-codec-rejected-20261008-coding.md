<!-- docs:metadata
title: "REJECTED execution candidate / GB10 Rust/native product coding.metal / none"
id: yvex.evaluation.qualification.deepseek-native-expert-codec-rejected-20261008-coding
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-expert-codec-rejected-20261008-coding.json
-->

# REJECTED execution candidate / GB10 Rust/native product coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-expert-codec-rejected-20261008-coding.json)

Target identity: `4c13dc600675fbfac92ccc00b6fe0dcd3ca67ab7c6c2fe1590bb53cd4909ff85`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.81833 | s | 3 | 0.817178–0.871235 | 0.00115196 |
| request.client-complete / coding.metal/turn-0 | 28.2683 | s | 3 | 28.224–28.505 | 0.0442731 |
| ttft.client-visible / coding.metal/turn-0 | 2.56333 | s | 3 | 2.50761–2.61975 | 0.0557255 |
| ttft.server / coding.metal/turn-0 | 1.69207 | s | 3 | 1.68925–1.80256 | 0.00282023 |
| final.first.server / coding.metal/turn-0 | 1.69207 | s | 3 | 1.68925–1.80256 | 0.00282023 |
| final.first.client / coding.metal/turn-0 | 2.56333 | s | 3 | 2.50761–2.61975 | 0.0557255 |
| final.phase-rate / coding.metal/turn-0 | 9.76761 | token/s | 3 | 9.67719–9.79639 | 0.0287822 |
| decode.post-first.committed / coding.metal/turn-0 | 9.89913 | token/s | 3 | 9.85149–9.9377 | 0.0385693 |
| prefill.uncached / coding.metal/turn-0 | 43.002 | token/s | 3 | 42.7461–43.4531 | 0.255896 |
| prefill.wall / coding.metal/turn-0 | 1.2325 | s | 3 | 1.21971–1.23988 | 0.00737827 |

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
| Draft phase | 2.26545 | s | 3 | 2.25967–2.36064 | 0.00577337 |
| Verification phase | 15.7439 | s | 3 | 15.6957–15.8201 | 0.0482409 |
| Speculative commit phase | 7.74199 | s | 3 | 7.70722–7.8159 | 0.0347696 |

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
| runtime_configuration | abe468ef724dee8e81411967bf7eb91af0d225931a21d86b02de10f1c5295453 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/expert-matrix-codec-candidate-coding-01/native/events.jsonl.

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
