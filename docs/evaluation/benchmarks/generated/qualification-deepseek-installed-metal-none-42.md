<!-- docs:metadata
title: "DeepSeek installed native coding.metal / none \u2014 repeated product control"
id: yvex.evaluation.qualification.deepseek-installed-metal-none-42
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-metal-none-42.json
-->

# DeepSeek installed native coding.metal / none — repeated product control

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-metal-none-42.json)

Target identity: `1b0a91dcda0fac068dc46f714fac5c42147c3e5347a318b1b37807ad2ed689c9`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.495169 | s | 4 | 0.430768–0.507368 | 0.00636589 |
| request.client-complete / coding.metal/turn-0 | 32.5268 | s | 4 | 32.4975–32.7114 | 0.0265052 |
| ttft.client-visible / coding.metal/turn-0 | 2.59491 | s | 4 | 2.53779–2.60773 | 0.0105392 |
| ttft.server / coding.metal/turn-0 | 2.10376 | s | 4 | 2.09095–2.10851 | 0.00401437 |
| final.first.server / coding.metal/turn-0 | 2.10376 | s | 4 | 2.09095–2.10851 | 0.00401437 |
| final.first.client / coding.metal/turn-0 | 2.59491 | s | 4 | 2.53779–2.60773 | 0.0105392 |
| final.phase-rate / coding.metal/turn-0 | 8.39739 | token/s | 4 | 8.35884–8.41548 | 0.010495 |
| decode.post-first.committed / coding.metal/turn-0 | 8.51133 | token/s | 4 | 8.46996–8.53018 | 0.0097359 |
| prefill.uncached / coding.metal/turn-0 | 33.6304 | token/s | 4 | 33.3647–33.7899 | 0.100032 |
| prefill.wall / coding.metal/turn-0 | 1.57596 | s | 4 | 1.56851–1.58851 | 0.00467109 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.metal/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 53 | token | 4 | 53–53 | 0 |
| Reused prefix | 0 | token | 4 | 0–0 | 0 |
| New prefill | 53 | token | 4 | 53–53 | 0 |
| Committed output | 256 | token | 4 | 256–256 | 0 |
| Reasoning | 0 | token | 4 | 0–0 | 0 |
| Final content | 256 | token | 4 | 256–256 | 0 |
| Draft cycles | 60 | cycle | 4 | 60–60 | 0 |
| Draft forwards | 60 | forward | 4 | 60–60 | 0 |
| Proposed | 300 | token | 4 | 300–300 | 0 |
| Selected verification | 298 | token | 4 | 298–298 | 0 |
| Target verifications | 60 | verification | 4 | 60–60 | 0 |
| Accepted draft | 136 | token | 4 | 136–136 | 0 |
| Rejected draft | 162 | token | 4 | 162–162 | 0 |
| Discarded draft | 2 | token | 4 | 2–2 | 0 |
| Correction/bonus | 60 | token | 4 | 60–60 | 0 |
| Per-sample mean accepted prefix | 2.26667 | token | 4 | 2.26667–2.26667 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 4 | 5–5 | 0 |
| Draft phase | 2.45913 | s | 4 | 2.45647–2.48084 | 0.00192283 |
| Verification phase | 19.0871 | s | 4 | 19.0577–19.1402 | 0.0216895 |
| Speculative commit phase | 8.39874 | s | 4 | 8.34293–8.44903 | 0.0369742 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |
| checkpoint-reference | UNQUALIFIED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |
| deployment-performance | UNQUALIFIED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |
| family-conformance | UNQUALIFIED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |
| product-path | CHARACTERIZED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |
| representation-quality | UNQUALIFIED | Repeated installed Rust/native product characterization; no independent representation-quality, hardware-exclusivity or throughput-target qualification;  |

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
| source_commit | a444bcdd384f6abfc79b07d1a26d93c17c4a97c0 |
| source_tree | 508e7cfeb5887fc123e200038d073ce72dc79049 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | f748ad6114858e5f1b6246f8bd2d6ade9a1743bd72e8e27b351d951c35b00bac |
| executable | e1ea8e02ad223a3fffb2ecc6839ee354658bd97a6e23c7e6b0651007d4ad3b4d |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 6438a5e3b5e9b01b2f43c05922eb6b05df9bfdae68d8aa74e3dbbb1c06118ba1 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-matrix-v41/coding.metal-none/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- LOCAL evidence, not YVEX-published model-quality, release or 20/700 target qualification.
- Exact clean producer source/build is bound externally; typed hardware/kernel facts unavailable, never inferred.
- Sampled process lists are not uninterrupted hardware exclusivity.
- No profiler or periodic smaps walk inside timed requests; external instrumentation declaration has bounded scope.
- Authored representative prompts are not official DeepSeek upstream encoding or inference vectors.
- Complete coding output is bounded at 256 tokens; no long-context or unlimited-completion claim.
- First fragment publication remains unavailable; internal and client-visible TTFT remain separate.
- All four resident-engine samples are retained, including the first workload sample; input histories are not pooled.
