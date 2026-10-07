<!-- docs:metadata
title: "Published producer: native C hash-table coding control"
id: yvex.evaluation.qualification.deepseek-published-native-coding-16
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-published-native-coding-16.json
-->

# Published producer: native C hash-table coding control

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-published-native-coding-16.json)

Target identity: `3983ced4306be4bd695917b703f306a424668da0da339da10d6e07009ed64742`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 1.43725 | s | 3 | 1.13072–1.55963 | 0.122373 |
| request.client-complete / coding.hash-table/turn-0 | 69.6657 | s | 3 | 68.9056–70.6531 | 0.76009 |
| ttft.client-visible / coding.hash-table/turn-0 | 6.37357 | s | 3 | 5.58943–6.42753 | 0.0539643 |
| ttft.server / coding.hash-table/turn-0 | 4.81403 | s | 3 | 4.45871–4.98991 | 0.175882 |
| final.first.server / coding.hash-table/turn-0 | 4.81403 | s | 3 | 4.45871–4.98991 | 0.175882 |
| final.first.client / coding.hash-table/turn-0 | 6.37357 | s | 3 | 5.58943–6.42753 | 0.0539643 |
| final.phase-rate / coding.hash-table/turn-0 | 3.95553 | token/s | 3 | 3.90076–3.95563 | 9.12211e-05 |
| decode.post-first.committed / coding.hash-table/turn-0 | 4.02748 | token/s | 3 | 3.97043–4.02902 | 0.00153519 |
| prefill.uncached / coding.hash-table/turn-0 | 9.15328 | token/s | 3 | 8.64401–10.1503 | 0.50927 |
| prefill.wall / coding.hash-table/turn-0 | 3.38676 | s | 3 | 3.05409–3.5863 | 0.199534 |

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
| Draft phase | 3.87674 | s | 3 | 3.83514–3.91032 | 0.0335822 |
| Verification phase | 41.3216 | s | 3 | 41.1704–41.5354 | 0.151211 |
| Speculative commit phase | 18.9518 | s | 3 | 18.8979–19.0474 | 0.0538624 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| draft-seconds / coding.hash-table/turn-0 | 3.8767401 | s | median server-authored DSpark proposal phase wall per complete turn; /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl |
| verification-seconds / coding.hash-table/turn-0 | 41.3216073 | s | median server-authored DSpark target verification phase wall per complete turn; /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl |
| commit-seconds / coding.hash-table/turn-0 | 18.9518111 | s | median server-authored speculative commit phase wall per complete turn; includes mandatory correction/bonus target extension, not just bookkeeping; /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl |

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
| specialization | 6c3e33a0ea61e9a371dc551d595b5a68e6f28e3059cde0ee49fc1370939c8350 |
| source_commit | 803dd98d4c54d7a26cb3c08def6b350be51b7179 |
| source_tree | 4ae815ad1669b4c95c302bb9b776e3671ee8f0a4 |
| source_delta | NOT RETAINED |
| build | 7592433e91b4d60830af95103dad508919670150be8fc631778284f8933e38c5 |
| executable | 6f931cf4d2873f8cfed63a996b088ac15325af63b14f29d8007573d0d1a85ae6 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 20cf9f38ef2a9415fee8df253a67af9d9758d54797268b3106e05dee13fb8caf |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-yai-recovery-20261006.JD7s2N/native-coding-published/native/events.jsonl.

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
- CPU software QA/builds were active during this series. No competing inference/GPU process was observed. Not a quiet comparison gate.
