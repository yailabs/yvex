<!-- docs:metadata
title: "REJECTED execution candidate / GB10 controlled native prefill.promessi-8192 / target-only"
id: yvex.evaluation.qualification.deepseek-rejected-interval-reduction-20261008-target-only-prefill-promessi-8192
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-rejected-interval-reduction-20261008-target-only-prefill-promessi-8192.json
-->

# REJECTED execution candidate / GB10 controlled native prefill.promessi-8192 / target-only

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-rejected-interval-reduction-20261008-target-only-prefill-promessi-8192.json)

Target identity: `88970c2ac91d2928f70f51f770b995dd52f6c65ec82b454b0c79b2fe5253d6f8`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.634486 | s | 3 | 0.622669–0.672485 | 0.0118167 |
| prefill.wall / prefill.promessi-8192/turn-0 | 92.2933 | s | 3 | 92.2543–92.3772 | 0.0389577 |
| ttft.server / prefill.promessi-8192/turn-0 | 92.4379 | s | 3 | 92.4043–92.5233 | 0.0335999 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 93.0769 | s | 3 | 93.0724–93.146 | 0.00449149 |
| request.client-complete / prefill.promessi-8192/turn-0 | 95.1107 | s | 3 | 95.0925–95.1713 | 0.0181499 |
| final.first.server / prefill.promessi-8192/turn-0 | 92.4379 | s | 3 | 92.4043–92.5233 | 0.0335999 |
| final.first.client / prefill.promessi-8192/turn-0 | 93.0769 | s | 3 | 93.0724–93.146 | 0.00449149 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 88.7605 | token/s | 3 | 88.6799–88.798 | 0.0374823 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 7.38048 | token/s | 3 | 7.33889–7.40339 | 0.0229024 |
| load.complete / prefill.promessi-8192/load | 5.33022 | s | 1 | 5.33022–5.33022 | 0 |

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
| source_commit | b66b8b1675a59a6365eb94920775bb9679fe27db |
| source_tree | 02831a098baf0759ddc3778bd54404a1725bf042 |
| source_delta | e0c770c6f06fab5715054542a8a4ce116b6d6568c491532ca9a4924fdadf5d4e |
| build | 7cc10372bd84186c4253af1e317a5af33792549e4a8eb2d01a2c65914031dfa5 |
| executable | bcc7e769b9e36d69426b82c41f4e7b6b28c55c07b1a8ee2076e012373bbc89dd |
| backend | cuda |
| backend_implementation | backend.cuda@b66b8b1675a59a6365eb94920775bb9679fe27db+e0c770c6f06fab5715054542a8a4ce116b6d6568c491532ca9a4924fdadf5d4e |
| kernel_bundle | 053f40d522f8b7f893027352f99f584f0371d6f750408e2f64787ab6b8319ab4 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | aad8fd079177f15d65256f9e5296f02a55cecf943340a40e8439d61c10e911f0 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **ttft.server**: turn start to first committed model-token callback. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **final.first.server**: server turn start to first source-classified final token. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: fresh; warm/cold: Loaded engine; no explicit warming; first post-load request and subsequent fresh sessions retained; first 0 repetitions excluded explicitly; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/prefill.promessi-8192.
- **load.complete**: load admission to execution-ready engine. Scope: Controlled engine/public-C host/native protocol v25; explicit greedy configuration; NOT installed Rust/chat or HTTP performance. Session: none; warm/cold: File/page-cache state uncontrolled; no cold-load claim; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/interval-reduction-controlled-01/yvex-prefill-target/load.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Timing is bound to one exact checkpoint/artifact/backend/device/configuration; no 20/700 gate or quality claim.
- This is a controlled public-C host consumed over native v25, not the installed Rust/chat product or HTTP adapter.
- Fresh sessions and exact uncached input counts; 16-token prefill controls do not publish sustained decode.
- Load is N=1 with uncontrolled file/cache state; it cannot qualify cold load latency or a residency speedup.
- Server first committed token and client-visible content are separate clocks; first fragment publication remains unavailable.
- Checkpoint/reference quality and successful high/maximum transitions are not qualified by non-thinking captures.
- Sampled clear lists cannot prove uninterrupted reservation. Raw observations remain independently reopenable.
- F32 certificate endpoint merging did not materially improve complete-model target-only decode or 2K/8K prefill; small native/speculative variation does not justify the retained complexity.
