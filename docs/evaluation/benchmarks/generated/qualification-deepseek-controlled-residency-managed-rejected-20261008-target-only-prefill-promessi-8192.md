<!-- docs:metadata
title: "REJECTED execution candidate / GB10 controlled native prefill.promessi-8192 / target-only"
id: yvex.evaluation.qualification.deepseek-controlled-residency-managed-rejected-20261008-target-only-prefill-promessi-8192
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-controlled-residency-managed-rejected-20261008-target-only-prefill-promessi-8192.json
-->

# REJECTED execution candidate / GB10 controlled native prefill.promessi-8192 / target-only

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-controlled-residency-managed-rejected-20261008-target-only-prefill-promessi-8192.json)

Target identity: `86133197bfc681ab3279fcc0d6f6d0a9f5efc90e536bc6f13f4cea83af5b2575`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.630462 | s | 3 | 0.629037–0.651725 | 0.0014249 |
| prefill.wall / prefill.promessi-8192/turn-0 | 93.1613 | s | 3 | 93.0709–93.373 | 0.0904401 |
| ttft.server / prefill.promessi-8192/turn-0 | 93.3105 | s | 3 | 93.2119–93.5258 | 0.0985164 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 93.941 | s | 3 | 93.8411–94.1772 | 0.0998532 |
| request.client-complete / prefill.promessi-8192/turn-0 | 95.9669 | s | 3 | 95.8393–96.1716 | 0.127606 |
| final.first.server / prefill.promessi-8192/turn-0 | 93.3105 | s | 3 | 93.2119–93.5258 | 0.0985164 |
| final.first.client / prefill.promessi-8192/turn-0 | 93.941 | s | 3 | 93.8411–94.1772 | 0.0998532 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 87.9335 | token/s | 3 | 87.7341–88.019 | 0.0854479 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 7.46425 | token/s | 3 | 7.36791–7.49118 | 0.0269371 |
| load.complete / prefill.promessi-8192/load | 105.314 | s | 1 | 105.314–105.314 | 0 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-8192/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 8192 | token | 3 | 8192–8192 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 8192 | token | 3 | 8192–8192 | 0 |
| Committed output | 16 | token | 3 | 16–16 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 16 | token | 3 | 16–16 | 0 |
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
| executable | 8f16b7c7c288fb59cb0fb45e2c0557faac31399efb4a10f7bbbef6d265cf864e |
| backend | cuda |
| backend_implementation | backend.cuda@f744f2b84ae71cf9e57f9c65ab23962e0d302dc2+ea011118be469307aeb4ab046dd04a56d508c98fd956b93a43cd329b1725f54f |
| kernel_bundle | 121843052bf4a7d8df5f2d850f807fbabd0b698c8da45281a936eb9f531da99b |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 79636328c77a9972855e804eb77a8a2a23bd10458074e176b688c14c78aba6cf |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | native stochastic=0, temperature=0; no stochastic draws |
| product_path | controlled-engine/public-C-host/native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **ttft.server**: turn start to first committed model-token callback. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **final.first.server**: server turn start to first source-classified final token. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Engine ready; no post-load conditioning; first post-load request and subsequent fresh sessions retained; file cache uncontrolled; load-residency policy explicitly identity-bound; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/prefill.promessi-8192.
- **load.complete**: load admission to execution-ready engine. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: none; warm/cold: File/page-cache state uncontrolled; no cold-load claim; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/managed-residency-window-02/yvex-prefill-target/load.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing is bound to one exact checkpoint/artifact/backend/device/configuration; no 20/700 gate or quality claim.
- This is a controlled public-C host consumed over native v25, not the installed Rust/chat product or HTTP adapter.
- Fresh sessions and exact uncached input counts; 16-token prefill controls do not publish sustained decode.
- Load is N=1 with uncontrolled file/cache state; it cannot qualify cold load latency or a residency speedup.
- Server first committed token and client-visible content are separate clocks; first fragment publication remains unavailable.
- Checkpoint/reference quality and successful high/maximum transitions are not qualified by non-thinking captures.
- Sampled clear lists cannot prove uninterrupted reservation. Raw observations remain independently reopenable.
- Managed-copy/prefetch did not demonstrate a complete-system Pareto improvement: coding target-only marginal increase, speculative coding decrease, 8K prefill decrease, and tighter physical memory/swap headroom. N=1 cache-uncontrolled load is not a causal loading comparison.
