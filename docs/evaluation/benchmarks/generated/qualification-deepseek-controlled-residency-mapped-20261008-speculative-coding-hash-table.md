<!-- docs:metadata
title: "GB10 controlled native coding.hash-table / speculative"
id: yvex.evaluation.qualification.deepseek-controlled-residency-mapped-20261008-speculative-coding-hash-table
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-controlled-residency-mapped-20261008-speculative-coding-hash-table.json
-->

# GB10 controlled native coding.hash-table / speculative

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-controlled-residency-mapped-20261008-speculative-coding-hash-table.json)

Target identity: `81a0eaf564ef7ba689a7515455862f6a496cf32a6448a3f16a26f2406b26c26b`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.632582 | s | 3 | 0.611124–0.687177 | 0.021458 |
| prefill.wall / coding.hash-table/turn-0 | 1.13223 | s | 3 | 1.13191–1.14408 | 0.000320121 |
| ttft.server / coding.hash-table/turn-0 | 1.67729 | s | 3 | 1.64395–1.77517 | 0.0333409 |
| ttft.client-visible / coding.hash-table/turn-0 | 2.36465 | s | 3 | 2.27653–2.38629 | 0.0216389 |
| request.client-complete / coding.hash-table/turn-0 | 24.3984 | s | 3 | 24.3145–24.6395 | 0.0839492 |
| final.first.server / coding.hash-table/turn-0 | 1.67729 | s | 3 | 1.64395–1.77517 | 0.0333409 |
| final.first.client / coding.hash-table/turn-0 | 2.36465 | s | 3 | 2.27653–2.38629 | 0.0216389 |
| prefill.uncached / coding.hash-table/turn-0 | 27.3796 | token/s | 3 | 27.096–27.3873 | 0.00774333 |
| decode.post-first.committed / coding.hash-table/turn-0 | 11.5717 | token/s | 3 | 11.46–11.5738 | 0.00209474 |
| final.phase-rate / coding.hash-table/turn-0 | 11.3388 | token/s | 3 | 11.1878–11.3534 | 0.014595 |
| load.complete / coding.hash-table/load | 7.63635 | s | 1 | 7.63635–7.63635 | 0 |

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
| Draft phase | 1.85218 | s | 3 | 1.84913–1.98176 | 0.0030517 |
| Verification phase | 14.1709 | s | 3 | 14.1111–14.2541 | 0.0598287 |
| Speculative commit phase | 6.1561 | s | 3 | 6.12397–6.22122 | 0.032135 |

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
| source_commit | f744f2b84ae71cf9e57f9c65ab23962e0d302dc2 |
| source_tree | 8f55a89cbe4754fcd468cf65e7667148eb538a24 |
| source_delta | ea011118be469307aeb4ab046dd04a56d508c98fd956b93a43cd329b1725f54f |
| build | c19953ae572a337b470dffd982eed348c2fc9cf2ba753d18ecdd29749d807714 |
| executable | 0b2f693df8ba9e46b41cba46ef58494fdbf0c93a85820047b8999cab97a074f8 |
| backend | cuda |
| backend_implementation | backend.cuda@f744f2b84ae71cf9e57f9c65ab23962e0d302dc2+ea011118be469307aeb4ab046dd04a56d508c98fd956b93a43cd329b1725f54f |
| kernel_bundle | 121843052bf4a7d8df5f2d850f807fbabd0b698c8da45281a936eb9f531da99b |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | a3c924580c05de1c075a834164418f47129f3f80d8e650149cf8575239e19fc8 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **ttft.server**: turn start to first committed model-token callback. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **final.first.server**: server turn start to first source-classified final token. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/coding.hash-table.
- **load.complete**: load admission to execution-ready engine. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: none; warm/cold: File/page-cache state uncontrolled; no cold-load claim; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/device-stage-window-01/yvex-coding-dspark/load.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing is bound to one exact checkpoint/artifact/backend/device/configuration; no 20/700 gate or quality claim.
- This is a controlled public-C host consumed over native v25, not the installed Rust/chat product or HTTP adapter.
- Fresh sessions and exact uncached input counts; 16-token prefill controls do not publish sustained decode.
- Load is N=1 with uncontrolled file/cache state; it cannot qualify cold load latency or a residency speedup.
- Server first committed token and client-visible content are separate clocks; first fragment publication remains unavailable.
- Checkpoint/reference quality and successful high/maximum transitions are not qualified by non-thinking captures.
- Sampled clear lists cannot prove uninterrupted reservation. Raw observations remain independently reopenable.
