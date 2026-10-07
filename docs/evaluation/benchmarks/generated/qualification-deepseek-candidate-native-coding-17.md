<!-- docs:metadata
title: "Candidate target-only: native C hash-table coding control"
id: yvex.evaluation.qualification.deepseek-candidate-native-coding-17
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-candidate-native-coding-17.json
-->

# Candidate target-only: native C hash-table coding control

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-candidate-native-coding-17.json)

Target identity: `cb8ba791a6c7ecc54af7b895beff2aab7e1cbc0830570071c7450cda02b274b7`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 1.46144 | s | 3 | 1.24047–1.49168 | 0.030237 |
| request.client-complete / coding.hash-table/turn-0 | 40.9666 | s | 3 | 39.3707–42.3356 | 1.36901 |
| ttft.client-visible / coding.hash-table/turn-0 | 3.17612 | s | 3 | 2.82722–3.23307 | 0.0569578 |
| ttft.server / coding.hash-table/turn-0 | 1.68448 | s | 3 | 1.58674–1.7716 | 0.0871197 |
| final.first.server / coding.hash-table/turn-0 | 1.68448 | s | 3 | 1.58674–1.7716 | 0.0871197 |
| final.first.client / coding.hash-table/turn-0 | 3.17612 | s | 3 | 2.82722–3.23307 | 0.0569578 |
| final.phase-rate / coding.hash-table/turn-0 | 6.68179 | token/s | 3 | 6.51862–7.01184 | 0.163164 |
| decode.post-first.committed / coding.hash-table/turn-0 | 6.68613 | token/s | 3 | 6.52144–7.04541 | 0.164693 |
| prefill.uncached / coding.hash-table/turn-0 | 21.9569 | token/s | 3 | 19.361–22.6532 | 0.696265 |
| prefill.wall / coding.hash-table/turn-0 | 1.41186 | s | 3 | 1.36846–1.60115 | 0.0433947 |

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
| Draft cycles | 0 | cycle | 3 | 0–0 | 0 |
| Draft forwards | 0 | forward | 3 | 0–0 | 0 |
| Proposed | 0 | token | 3 | 0–0 | 0 |
| Selected verification | 0 | token | 3 | 0–0 | 0 |
| Target verifications | 0 | verification | 3 | 0–0 | 0 |
| Accepted draft | 0 | token | 3 | 0–0 | 0 |
| Rejected draft | 0 | token | 3 | 0–0 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 0 | token | 3 | 0–0 | 0 |
| Per-sample mean accepted prefix | 0 | token | 3 | 0–0 | 0 |
| Per-sample maximum accepted prefix | 0 | token | 3 | 0–0 | 0 |
| Draft phase | 0 | s | 3 | 0–0 | 0 |
| Verification phase | 0 | s | 3 | 0–0 | 0 |
| Speculative commit phase | 0 | s | 3 | 0–0 | 0 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | This coding capture does not independently qualify backend-execution;  |
| checkpoint-reference | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |
| deployment-performance | CHARACTERIZED | Three complete native protocol v24 coding requests with 256 committed tokens, exact producer configuration; no universal product or release rate;  |
| family-conformance | UNQUALIFIED | This coding capture does not independently qualify family-conformance;  |
| product-path | CHARACTERIZED | Three complete native protocol v24 coding requests with 256 committed tokens, exact producer configuration; no universal product or release rate;  |
| representation-quality | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |

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
| source_commit | 803dd98d4c54d7a26cb3c08def6b350be51b7179 |
| source_tree | 4ae815ad1669b4c95c302bb9b776e3671ee8f0a4 |
| source_delta | 05874f2e9dec2e4cacba3bf85e83372ca81930820a4416b6f43ef2fa66de1741 |
| build | 01210aceb34c286c52cb9c90f27eea48291fb48f14605bc8d3f85b65bf78dc63 |
| executable | fff65135a988b0cf0dc8482865c4208621133a45345326a249e75b424f64e8ca |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 4cb79308f91a92215e274cb383a325f78f2cab5bc420277d526c115770cfc036 |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-current-hash-target-04/case-0/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- YVEX-authored external-comparison coding prompt, not an official DeepSeek vector. Checkpoint-matched quality, original higher-precision conformance and 20/700 targets are not earned.
- Fresh sessions, 31 rendered/new input tokens, 256 committed output tokens, reasoning none and explicit greedy sampling. Tiny prefill is not a 2K/8K throughput claim.
- Published producer and unpublished candidate differ in source/build, strategy, context, chunk, capacity plan and specialization. These rows must not be pooled or presented as a causal optimization comparison.
- Producer executable identity is distinct from measurement client identity. Native protocol v24 measurement is not HTTP or terminal redraw timing.
- Server first-token callback, client first visible content and complete response are different clocks; first fragment publication remains unavailable.
- Sampled CUDA process lists do not prove uninterrupted exclusive hardware access; observation overhead/cadence and exact raw evidence remain retained.
- Reasoning high/maximum and reused histories are not characterized by this non-thinking fresh-session series.
- Backend/kernel bundle and runtime/toolkit identities remain unavailable in this import. No plane is relabeled QUALIFIED from successful terminal completion.
- Scoped file-cache observation found zero pages resident before load; load 84.303s is a separate cold-file diagnostic, not pooled with a warmed published-service load.
