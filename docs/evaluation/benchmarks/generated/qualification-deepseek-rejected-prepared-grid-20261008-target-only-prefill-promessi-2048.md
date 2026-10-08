<!-- docs:metadata
title: "REJECTED execution candidate / GB10 controlled native prefill.promessi-2048 / target-only"
id: yvex.evaluation.qualification.deepseek-rejected-prepared-grid-20261008-target-only-prefill-promessi-2048
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-rejected-prepared-grid-20261008-target-only-prefill-promessi-2048.json
-->

# REJECTED execution candidate / GB10 controlled native prefill.promessi-2048 / target-only

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-rejected-prepared-grid-20261008-target-only-prefill-promessi-2048.json)

Target identity: `efa7a253bdff97a0a3827c9bc661bb2419f5dd5b8873ac90a3e9b9c0bba47625`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.625527 | s | 3 | 0.615564–0.649593 | 0.00996279 |
| prefill.wall / prefill.promessi-2048/turn-0 | 20.0874 | s | 3 | 20.0397–20.1197 | 0.0322974 |
| ttft.server / prefill.promessi-2048/turn-0 | 20.2312 | s | 3 | 20.1739–20.2538 | 0.0225442 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 20.8468 | s | 3 | 20.8235–20.8793 | 0.0233086 |
| request.client-complete / prefill.promessi-2048/turn-0 | 22.7652 | s | 3 | 22.7387–22.7903 | 0.0251141 |
| final.first.server / prefill.promessi-2048/turn-0 | 20.2312 | s | 3 | 20.1739–20.2538 | 0.0225442 |
| final.first.client / prefill.promessi-2048/turn-0 | 20.8468 | s | 3 | 20.8235–20.8793 | 0.0233086 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 101.954 | token/s | 3 | 101.791–102.197 | 0.163663 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 7.81237 | token/s | 3 | 7.76505–7.82894 | 0.0165791 |
| load.complete / prefill.promessi-2048/load | 7.14753 | s | 1 | 7.14753–7.14753 | 0 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-2048/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 2048 | token | 3 | 2048–2048 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 2048 | token | 3 | 2048–2048 | 0 |
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
| source_commit | b66b8b1675a59a6365eb94920775bb9679fe27db |
| source_tree | 02831a098baf0759ddc3778bd54404a1725bf042 |
| source_delta | 9cc50bce81bee0a59c429f05989d5945e1b52ec5fa482f4b526a879962386b28 |
| build | 4e689c3f333e751231c93fe059fae6c1503a8e9e6a1a69d30c1ddf02dee3d68e |
| executable | bf8d9e1eac796b0c7a28538c2762c38a1d88a0a131b0a679d5793f2e98af2f9a |
| backend | cuda |
| backend_implementation | backend.cuda@b66b8b1675a59a6365eb94920775bb9679fe27db+9cc50bce81bee0a59c429f05989d5945e1b52ec5fa482f4b526a879962386b28 |
| kernel_bundle | 306fd14d3d15f1b051b91637215f53c856805025a7ac110a511b122747d5f34a |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | c5de4039d1d5bd08000fe576bc28a86bbe7a51de36a3fddd62fc5c0ba22de602 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **ttft.server**: turn start to first committed model-token callback. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **final.first.server**: server turn start to first source-classified final token. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/prefill.promessi-2048.
- **load.complete**: load admission to execution-ready engine. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: none; warm/cold: File/page-cache state uncontrolled; no cold-load claim; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/prepared-grid-controlled-02/yvex-prefill-target/load.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing is bound to one exact checkpoint/artifact/backend/device/configuration; no 20/700 gate or quality claim.
- This is a controlled public-C host consumed over native v25, not the installed Rust/chat product or HTTP adapter.
- Fresh sessions and exact uncached input counts; 16-token prefill controls do not publish sustained decode.
- Load is N=1 with uncontrolled file/cache state; it cannot qualify cold load latency or a residency speedup.
- Server first committed token and client-visible content are separate clocks; first fragment publication remains unavailable.
- Checkpoint/reference quality and successful high/maximum transitions are not qualified by non-thinking captures.
- Sampled clear lists cannot prove uninterrupted reservation. Raw observations remain independently reopenable.
- Complete-model N=3 comparison shows no material benefit over narrow-Q8 matrix predecessor; native coding gain is about 0.7%, target-only coding is neutral/slower, and 2K/8K prefill is neutral. A larger per-block shared table without a material model benefit is not retained.
