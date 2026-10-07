<!-- docs:metadata
title: "DeepSeek installed native conversation.coding / none \u2014 repeated product control"
id: yvex.evaluation.qualification.deepseek-installed-conversation-none-42
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-conversation-none-42.json
-->

# DeepSeek installed native conversation.coding / none — repeated product control

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-conversation-none-42.json)

Target identity: `1b0a91dcda0fac068dc46f714fac5c42147c3e5347a318b1b37807ad2ed689c9`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / conversation.coding/turn-0 | 0.493377 | s | 4 | 0.47292–0.501575 | 0.00609168 |
| request.client-complete / conversation.coding/turn-0 | 1.97269 | s | 4 | 1.95356–1.98874 | 0.0103975 |
| ttft.client-visible / conversation.coding/turn-0 | 1.60374 | s | 4 | 1.59041–1.62404 | 0.0109046 |
| ttft.server / conversation.coding/turn-0 | 1.11404 | s | 4 | 1.10587–1.12667 | 0.00578477 |
| final.first.server / conversation.coding/turn-0 | 1.11404 | s | 4 | 1.10587–1.12667 | 0.00578477 |
| final.first.client / conversation.coding/turn-0 | 1.60374 | s | 4 | 1.59041–1.62404 | 0.0109046 |
| final.phase-rate / conversation.coding/turn-0 | 11.5573 | token/s | 4 | 11.4817–11.5994 | 0.0281047 |
| prefill.uncached / conversation.coding/turn-0 | 9.81333 | token/s | 4 | 9.54232–9.84852 | 0.020548 |
| prefill.wall / conversation.coding/turn-0 | 0.611414 | s | 4 | 0.609229–0.628778 | 0.00127642 |
| admission.client / conversation.coding/turn-1 | 0.00101921 | s | 4 | 0.000878143–0.00128166 | 0.000123911 |
| request.client-complete / conversation.coding/turn-1 | 31.705 | s | 4 | 31.4429–31.7752 | 0.0513512 |
| ttft.client-visible / conversation.coding/turn-1 | 1.07161 | s | 4 | 1.06439–1.08432 | 0.00592457 |
| ttft.server / conversation.coding/turn-1 | 1.07057 | s | 4 | 1.06332–1.0829 | 0.00593436 |
| final.first.server / conversation.coding/turn-1 | 1.07057 | s | 4 | 1.06332–1.0829 | 0.00593436 |
| final.first.client / conversation.coding/turn-1 | 1.07161 | s | 4 | 1.06439–1.08432 | 0.00592457 |
| final.phase-rate / conversation.coding/turn-1 | 8.23018 | token/s | 4 | 8.21222–8.29911 | 0.0127931 |
| decode.post-first.committed / conversation.coding/turn-1 | 8.32474 | token/s | 4 | 8.30913–8.39458 | 0.0115932 |
| prefill.wall / conversation.coding/turn-1 | 0.595902 | s | 4 | 0.593057–0.600414 | 0.00279209 |
| admission.client / conversation.coding/turn-2 | 0.00109221 | s | 4 | 0.000961726–0.00159675 | 8.5536e-05 |
| request.client-complete / conversation.coding/turn-2 | 33.4006 | s | 4 | 33.3596–33.6239 | 0.0349439 |
| ttft.client-visible / conversation.coding/turn-2 | 1.40713 | s | 4 | 1.40306–1.41619 | 0.00302229 |
| ttft.server / conversation.coding/turn-2 | 1.4059 | s | 4 | 1.40205–1.41448 | 0.00289644 |
| final.first.server / conversation.coding/turn-2 | 1.4059 | s | 4 | 1.40205–1.41448 | 0.00289644 |
| final.first.client / conversation.coding/turn-2 | 1.40713 | s | 4 | 1.40306–1.41619 | 0.00302229 |
| final.phase-rate / conversation.coding/turn-2 | 7.86844 | token/s | 4 | 7.81468–7.87899 | 0.00872526 |
| decode.post-first.committed / conversation.coding/turn-2 | 7.97181 | token/s | 4 | 7.91469–7.98166 | 0.00782647 |
| prefill.wall / conversation.coding/turn-2 | 0.859902 | s | 4 | 0.85983–0.862725 | 4.615e-05 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### conversation.coding/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 6 | token | 4 | 6–6 | 0 |
| Reused prefix | 0 | token | 4 | 0–0 | 0 |
| New prefill | 6 | token | 4 | 6–6 | 0 |
| Committed output | 10 | token | 4 | 10–10 | 0 |
| Reasoning | 0 | token | 4 | 0–0 | 0 |
| Final content | 10 | token | 4 | 10–10 | 0 |
| Draft cycles | 2 | cycle | 4 | 2–2 | 0 |
| Draft forwards | 2 | forward | 4 | 2–2 | 0 |
| Proposed | 10 | token | 4 | 10–10 | 0 |
| Selected verification | 10 | token | 4 | 10–10 | 0 |
| Target verifications | 2 | verification | 4 | 2–2 | 0 |
| Accepted draft | 7 | token | 4 | 7–7 | 0 |
| Rejected draft | 3 | token | 4 | 3–3 | 0 |
| Discarded draft | 0 | token | 4 | 0–0 | 0 |
| Correction/bonus | 2 | token | 4 | 2–2 | 0 |
| Per-sample mean accepted prefix | 3.5 | token | 4 | 3.5–3.5 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 4 | 5–5 | 0 |
| Draft phase | 0.074213 | s | 4 | 0.0730815–0.0742955 | 6.3472e-05 |
| Verification phase | 0.616361 | s | 4 | 0.614856–0.624805 | 0.00132604 |
| Speculative commit phase | 0.157056 | s | 4 | 0.15623–0.158094 | 0.000710254 |
### conversation.coding/turn-1

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 25 | token | 4 | 25–25 | 0 |
| Reused prefix | 16 | token | 4 | 16–16 | 0 |
| New prefill | 9 | token | 4 | 9–9 | 0 |
| Committed output | 256 | token | 4 | 256–256 | 0 |
| Reasoning | 0 | token | 4 | 0–0 | 0 |
| Final content | 256 | token | 4 | 256–256 | 0 |
| Draft cycles | 61 | cycle | 4 | 61–61 | 0 |
| Draft forwards | 61 | forward | 4 | 61–61 | 0 |
| Proposed | 305 | token | 4 | 305–305 | 0 |
| Selected verification | 301 | token | 4 | 301–301 | 0 |
| Target verifications | 61 | verification | 4 | 61–61 | 0 |
| Accepted draft | 134 | token | 4 | 134–134 | 0 |
| Rejected draft | 167 | token | 4 | 167–167 | 0 |
| Discarded draft | 4 | token | 4 | 4–4 | 0 |
| Correction/bonus | 61 | token | 4 | 61–61 | 0 |
| Per-sample mean accepted prefix | 2.19672 | token | 4 | 2.19672–2.19672 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 4 | 5–5 | 0 |
| Draft phase | 2.56254 | s | 4 | 2.5489–2.5861 | 0.00769697 |
| Verification phase | 19.3008 | s | 4 | 19.206–19.3296 | 0.0230598 |
| Speculative commit phase | 8.67194 | s | 4 | 8.52851–8.70167 | 0.0203672 |
### conversation.coding/turn-2

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 308 | token | 4 | 308–308 | 0 |
| Reused prefix | 281 | token | 4 | 281–281 | 0 |
| New prefill | 27 | token | 4 | 27–27 | 0 |
| Committed output | 256 | token | 4 | 256–256 | 0 |
| Reasoning | 0 | token | 4 | 0–0 | 0 |
| Final content | 256 | token | 4 | 256–256 | 0 |
| Draft cycles | 57 | cycle | 4 | 57–57 | 0 |
| Draft forwards | 57 | forward | 4 | 57–57 | 0 |
| Proposed | 285 | token | 4 | 285–285 | 0 |
| Selected verification | 285 | token | 4 | 285–285 | 0 |
| Target verifications | 57 | verification | 4 | 57–57 | 0 |
| Accepted draft | 142 | token | 4 | 142–142 | 0 |
| Rejected draft | 143 | token | 4 | 143–143 | 0 |
| Discarded draft | 0 | token | 4 | 0–0 | 0 |
| Correction/bonus | 57 | token | 4 | 57–57 | 0 |
| Per-sample mean accepted prefix | 2.49123 | token | 4 | 2.49123–2.49123 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 4 | 5–5 | 0 |
| Draft phase | 2.69363 | s | 4 | 2.67806–2.70115 | 0.00645713 |
| Verification phase | 19.291 | s | 4 | 19.2618–19.4376 | 0.0213314 |
| Speculative commit phase | 9.98013 | s | 4 | 9.94593–10.0698 | 0.0200909 |

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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: reused; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/conversation.coding-none/native/events.jsonl.

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
- The ten-token first turn supplies latency, not sustained decode; reused prefill supplies wall time, not uncached throughput.
