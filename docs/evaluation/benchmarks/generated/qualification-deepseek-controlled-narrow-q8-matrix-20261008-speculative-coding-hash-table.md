<!-- docs:metadata
title: "GB10 controlled native coding.hash-table / speculative"
id: yvex.evaluation.qualification.deepseek-controlled-narrow-q8-matrix-20261008-speculative-coding-hash-table
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-controlled-narrow-q8-matrix-20261008-speculative-coding-hash-table.json
-->

# GB10 controlled native coding.hash-table / speculative

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-controlled-narrow-q8-matrix-20261008-speculative-coding-hash-table.json)

Target identity: `21633d4483a894a3e363be1a6c07557998300701c5b890d4602e6e6fc62809b0`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.638029 | s | 3 | 0.619585–0.690952 | 0.0184444 |
| prefill.wall / coding.hash-table/turn-0 | 1.0816 | s | 3 | 1.07874–1.08531 | 0.00286343 |
| ttft.server / coding.hash-table/turn-0 | 1.59944 | s | 3 | 1.58992–1.70521 | 0.0095165 |
| ttft.client-visible / coding.hash-table/turn-0 | 2.28042 | s | 3 | 2.23757–2.3249 | 0.0428496 |
| request.client-complete / coding.hash-table/turn-0 | 23.4736 | s | 3 | 23.4537–23.7254 | 0.0199159 |
| final.first.server / coding.hash-table/turn-0 | 1.59944 | s | 3 | 1.58992–1.70521 | 0.0095165 |
| final.first.client / coding.hash-table/turn-0 | 2.28042 | s | 3 | 2.23757–2.3249 | 0.0428496 |
| prefill.uncached / coding.hash-table/turn-0 | 28.6611 | token/s | 3 | 28.5633–28.7372 | 0.0760786 |
| decode.post-first.committed / coding.hash-table/turn-0 | 12.02 | token/s | 3 | 11.9164–12.0334 | 0.0134272 |
| final.phase-rate / coding.hash-table/turn-0 | 11.7797 | token/s | 3 | 11.6265–11.7961 | 0.0164639 |
| load.complete / coding.hash-table/load | 5.59916 | s | 1 | 5.59916–5.59916 | 0 |

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
| Draft phase | 1.87668 | s | 3 | 1.86191–1.9636 | 0.0147698 |
| Verification phase | 13.276 | s | 3 | 13.2527–13.3672 | 0.0232917 |
| Speculative commit phase | 6.15827 | s | 3 | 6.14983–6.26124 | 0.00844236 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not independently qualified by this timing capture;  |
| checkpoint-reference | BLOCKED | Not independently qualified by this timing capture; No admitted independent checkpoint-matched full-model numerical/quality comparison in this timing series |
| representation-quality | BLOCKED | Not independently qualified by this timing capture; No admitted independent checkpoint-matched full-model numerical/quality comparison in this timing series |
| backend-execution | UNQUALIFIED | Not independently qualified by this timing capture;  |
| deployment-performance | CHARACTERIZED | Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance;  |
| product-path | CHARACTERIZED | Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance;  |

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
| source_commit | 46d5476930902fe2553bcbf8132636cad607bcd1 |
| source_tree | b27de27964233f1f475ae4b11bb045cc85753e65 |
| source_delta | 1bc378817ac310e08a905c9c4b0a0f48be170b169a6fa4cccc43ce7030bada5b |
| build | bb1812ece26f0d286da45bc6d31e0232ee10d0bd8828bd8001a0684141c29078 |
| executable | 667a7ba754a747b1bf6eb30bd793eb10dbccf72c7d4e692c1640d9d4cba734e8 |
| backend | cuda |
| backend_implementation | backend.cuda@46d5476930902fe2553bcbf8132636cad607bcd1+1bc378817ac310e08a905c9c4b0a0f48be170b169a6fa4cccc43ce7030bada5b |
| kernel_bundle | 8a83d53f5d106266ee11032d4967f2f3523e2528ea70f98759cc9e5c34ba16ab |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | ea803c973fc994828c81c0572a1a241003e2ea82125fe9eab9d39ad1adea523e |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | native stochastic=0, temperature=0; no stochastic draws |
| product_path | controlled-engine/public-C-host/native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **ttft.server**: turn start to first committed model-token callback. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **final.first.server**: server turn start to first source-classified final token. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/coding.hash-table.
- **load.complete**: load admission to execution-ready engine. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: none; warm/cold: File/page-cache state uncontrolled; no cold-load claim; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/narrow-q8-matrix-controlled-01/yvex-coding-dspark/load.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing is bound to one exact checkpoint/artifact/backend/device/configuration; no 20/700 gate or quality claim.
- This is a controlled public-C host consumed over native v25, not the installed Rust/chat product or HTTP adapter.
- Fresh sessions and exact uncached input counts; 16-token prefill controls do not publish sustained decode.
- Load is N=1 with uncontrolled file/cache state; it cannot qualify cold load latency or a residency speedup.
- Server first committed token and client-visible content are separate clocks; first fragment publication remains unavailable.
- Checkpoint/reference quality and successful high/maximum transitions are not qualified by non-thinking captures.
- Sampled clear lists cannot prove uninterrupted reservation. Raw observations remain independently reopenable.
