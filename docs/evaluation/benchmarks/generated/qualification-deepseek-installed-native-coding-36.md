<!-- docs:metadata
title: "Installed Rust/native coding control: exact mixed artifact, DSpark and chunk64"
id: yvex.evaluation.qualification.deepseek-installed-native-coding-36
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-native-coding-36.json
-->

# Installed Rust/native coding control: exact mixed artifact, DSpark and chunk64

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-native-coding-36.json)

Target identity: `5883df3513715bbfad7e50e1fcaf099db9bc2ddefa25b071ba5d97497e609263`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.50002 | s | 3 | 0.495781–0.502276 | 0.00225525 |
| request.client-complete / coding.hash-table/turn-0 | 24.8642 | s | 3 | 24.841–24.8872 | 0.0230332 |
| ttft.client-visible / coding.hash-table/turn-0 | 2.21658 | s | 3 | 2.20102–2.21971 | 0.00312585 |
| ttft.server / coding.hash-table/turn-0 | 1.71658 | s | 3 | 1.69866–1.72395 | 0.00736911 |
| final.first.server / coding.hash-table/turn-0 | 1.71658 | s | 3 | 1.69866–1.72395 | 0.00736911 |
| final.first.client / coding.hash-table/turn-0 | 2.21658 | s | 3 | 2.20102–2.21971 | 0.00312585 |
| final.phase-rate / coding.hash-table/turn-0 | 11.0457 | token/s | 3 | 11.0384–11.0536 | 0.00727795 |
| decode.post-first.committed / coding.hash-table/turn-0 | 11.2619 | token/s | 3 | 11.2489–11.2641 | 0.00219995 |
| prefill.uncached / coding.hash-table/turn-0 | 26.0464 | token/s | 3 | 25.9718–26.337 | 0.0746672 |
| prefill.wall / coding.hash-table/turn-0 | 1.19018 | s | 3 | 1.17705–1.1936 | 0.0034217 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.hash-table/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 31 | token | 4 | 31–31 | 0 |
| Reused prefix | 0 | token | 4 | 0–0 | 0 |
| New prefill | 31 | token | 4 | 31–31 | 0 |
| Committed output | 256 | token | 4 | 256–256 | 0 |
| Reasoning | 0 | token | 4 | 0–0 | 0 |
| Final content | 256 | token | 4 | 256–256 | 0 |
| Draft cycles | 46 | cycle | 4 | 46–46 | 0 |
| Draft forwards | 46 | forward | 4 | 46–46 | 0 |
| Proposed | 230 | token | 4 | 230–230 | 0 |
| Selected verification | 227 | token | 4 | 227–227 | 0 |
| Target verifications | 46 | verification | 4 | 46–46 | 0 |
| Accepted draft | 164 | token | 4 | 164–164 | 0 |
| Rejected draft | 63 | token | 4 | 63–63 | 0 |
| Discarded draft | 3 | token | 4 | 3–3 | 0 |
| Correction/bonus | 46 | token | 4 | 46–46 | 0 |
| Per-sample mean accepted prefix | 3.56522 | token | 4 | 3.56522–3.56522 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 4 | 5–5 | 0 |
| Draft phase | 1.87612 | s | 4 | 1.86642–1.95956 | 0.00507302 |
| Verification phase | 14.5132 | s | 4 | 14.4934–14.5936 | 0.0122501 |
| Speculative commit phase | 6.36665 | s | 4 | 6.35672–6.45737 | 0.00536836 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| checkpoint-reference | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| deployment-performance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Local native measurement only; independent reference and producer provenance incomplete;  |
| representation-quality | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |

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
| runtime_configuration | 5d7575fcebc1ddc05607efa1d09e4c623ec175c55db364429509455a875fb46b |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident-engine; first coding sample retained separately; subsequent three samples; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-native-coding-v36/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not YVEX-published qualification or a model-quality claim.
- First fragment publication and reasoning-to-final transition not measured.
- Different generated histories are separate input groups, never pooled or treated as deterministic agreement.
- Advisory locks do not prove hardware exclusivity; external resource observation is required.
- External installed-Host source/build witness authenticates the declared producer; hardware/kernel and uninterrupted contention evidence remain unavailable, never inferred.
- No independent quality oracle. First coding sample is retained separately; subsequent three samples form the declared warm series.
