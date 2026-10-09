<!-- docs:metadata
title: "GB10 Rust/native product coding.metal / high"
id: yvex.evaluation.qualification.deepseek-native-qtype-combined-long-coding-high-16k-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-qtype-combined-long-coding-high-16k-20261009.json
-->

# GB10 Rust/native product coding.metal / high

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-qtype-combined-long-coding-high-16k-20261009.json)

Target identity: `0bb7277d7e4737911ecb64e399a6461b59f3efac0200894b62e4cac853db1923`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.773758 | s | 3 | 0.747827–0.781938 | 0.00818085 |
| request.client-complete / coding.metal/turn-0 | 295.028 | s | 3 | 294.738–295.108 | 0.0794028 |
| ttft.client-visible / coding.metal/turn-0 | 2.36482 | s | 3 | 2.35549–2.42694 | 0.00933515 |
| ttft.server / coding.metal/turn-0 | 1.58288 | s | 3 | 1.58171–1.6791 | 0.00116574 |
| reasoning.first.server / coding.metal/turn-0 | 1.58288 | s | 3 | 1.58171–1.6791 | 0.00116574 |
| reasoning.first.client / coding.metal/turn-0 | 2.36482 | s | 3 | 2.35549–2.42694 | 0.00933515 |
| final.first.server / coding.metal/turn-0 | 180.899 | s | 3 | 180.628–183.016 | 0.271172 |
| final.first.client / coding.metal/turn-0 | 181.673 | s | 3 | 181.41–183.764 | 0.262979 |
| reasoning.phase-rate / coding.metal/turn-0 | 8.39419 | token/s | 3 | 8.29569–8.40692 | 0.0127257 |
| final.phase-rate / coding.metal/turn-0 | 7.61623 | token/s | 3 | 7.6091–7.75763 | 0.00713401 |
| decode.post-first.committed / coding.metal/turn-0 | 8.10712 | token/s | 3 | 8.10291–8.1134 | 0.00420805 |
| prefill.uncached / coding.metal/turn-0 | 46.8232 | token/s | 3 | 46.7842–47.5975 | 0.0390158 |
| prefill.wall / coding.metal/turn-0 | 1.13192 | s | 3 | 1.1135–1.13286 | 0.000943967 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.metal/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 53 | token | 3 | 53–53 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 53 | token | 3 | 53–53 | 0 |
| Committed output | 2373 | token | 3 | 2373–2373 | 0 |
| Reasoning | 1509 | token | 3 | 1509–1509 | 0 |
| Final content | 863 | token | 3 | 863–863 | 0 |
| Draft cycles | 345 | cycle | 3 | 345–345 | 0 |
| Draft forwards | 345 | forward | 3 | 345–345 | 0 |
| Proposed | 1725 | token | 3 | 1725–1725 | 0 |
| Selected verification | 1725 | token | 3 | 1725–1725 | 0 |
| Target verifications | 345 | verification | 3 | 345–345 | 0 |
| Accepted draft | 821 | token | 3 | 821–821 | 0 |
| Rejected draft | 904 | token | 3 | 904–904 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 345 | token | 3 | 345–345 | 0 |
| Per-sample mean accepted prefix | 2.37971 | token | 3 | 2.37971–2.37971 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 14.1652 | s | 3 | 14.1534–14.221 | 0.0117832 |
| Verification phase | 101.276 | s | 3 | 101.233–102.755 | 0.0433619 |
| Speculative commit phase | 61.0737 | s | 3 | 60.8502–61.7431 | 0.223536 |

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
| source_commit | 46546a46f1a2ab9b5d08ddff13c909880b4d990c |
| source_tree | 6dfcdbb752bf22cad28dd0d6036606059352bd8b |
| source_delta | 3ba765d1847e8226b19edadb7e2e89d59f76d7767977636901e265551a522633 |
| build | f8bec005ba83018e8222f8d8179ca58b1c925cd66d8d822c76194e71a06ce499 |
| executable | 015f0562ae4eefb4d778125353037efe7099525010134e0f07a1dd417853c046 |
| backend | cuda |
| backend_implementation | backend.cuda@46546a46f1a2ab9b5d08ddff13c909880b4d990c+3ba765d1847e8226b19edadb7e2e89d59f76d7767977636901e265551a522633 |
| kernel_bundle | 583bd64c7e0e27ec7230757b82bb535f386114dea5912e70702eef94587666bc |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | b766c414b9c665db434ee7f7377a6d3ab45a0e47cda8038a5c00fc120d5eb1f6 |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | high |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | b048ce89750a4c93f9e6f667a101eca271c05295fef932724b8f4c7b8dec0176 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **reasoning.first.server**: server turn start to first source-classified reasoning token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **reasoning.first.client**: client dispatch including connect to first nonempty reasoning fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **reasoning.phase-rate**: source-classified reasoning tokens / server phase from prefill completion to reasoning boundary or decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/qtype-combined-native-long-coding-high-16k-20261009-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Sampling selection is greedy; resolved facts in receipt; exact temperature/stochastic fields are retained per observation. Native and HTTP defaults are not interchangeable.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- This explicit reasoning/speculative target uses the admitted source-boundary sub-policy: ordinary target decoding follows the reasoning terminator. A boundary speculative block can contain final-channel tokens; final-channel count is not an exact target-only work counter.
- All three fresh sessions reached natural EOS. This 4096-bound, 16384-context series is separate from 256-bound coding and the installed 32768/chunk-64 product.
