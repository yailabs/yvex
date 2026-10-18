<!-- docs:metadata
title: "0731 MXFP4/Q2 native online64 realization: speculative / coding.hash-table / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-online64-speculative-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-online64-speculative-coding-hash-table-20261012.json
-->

# 0731 MXFP4/Q2 native online64 realization: speculative / coding.hash-table / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-online64-speculative-coding-hash-table-20261012.json)

Target identity: `27c31a3b051877ef0d0242fd5755f8f1646c2ca486ff0374bce98ce43f4ea58e`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.578414 | s | 3 | 0.569142–0.607946 | 0.00927194 |
| request.client-complete / coding.hash-table/turn-0 | 18.6838 | s | 3 | 18.6723–18.8766 | 0.0115248 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.74955 | s | 3 | 1.74313–1.91002 | 0.00642259 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.431501 | s | 3 | 0.430409–0.440997 | 0.00109215 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0664067 | s | 3 | 0.0663867–0.066534 | 1.99867e-05 |
| ttft.server / coding.hash-table/turn-0 | 1.17396 | s | 3 | 1.17114–1.30208 | 0.00281513 |
| final.first.server / coding.hash-table/turn-0 | 1.17396 | s | 3 | 1.17114–1.30208 | 0.00281513 |
| final.first.client / coding.hash-table/turn-0 | 1.74955 | s | 3 | 1.74313–1.91002 | 0.00642259 |
| final.phase-rate / coding.hash-table/turn-0 | 14.7412 | token/s | 3 | 14.644–14.7519 | 0.010681 |
| decode.post-first.committed / coding.hash-table/turn-0 | 15.0587 | token/s | 3 | 15.0299–15.0634 | 0.00461524 |
| prefill.uncached / coding.hash-table/turn-0 | 41.4098 | token/s | 3 | 39.4148–41.9824 | 0.572693 |
| prefill.wall / coding.hash-table/turn-0 | 0.748616 | s | 3 | 0.738404–0.786507 | 0.0102121 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.04097006e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/memory-post-load-4096.json |
| post-load.pss / coding.hash-table | 1.04087296e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/memory-post-load-4096.json |
| post-load.pss_file / coding.hash-table | 1.03846025e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/memory-post-load-4096.json |
| post-load.pss_anon / coding.hash-table | 197033984 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/memory-post-load-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/memory-post-load-4096.json |
| load.client / coding.hash-table | 5.61056778 | s | One load command to ready; artifact authentication already read file pages; not cold-storage latency; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/load-4096.json |
| native.prompt_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.0 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.0 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.0 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.0 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.0 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.0 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.0 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.0 / coding.hash-table | 1.19267463 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.0 / coding.hash-table | 10.7893192 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.0 / coding.hash-table | 5.15436072 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.1 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.1 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.1 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.1 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.1 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.1 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.1 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.1 / coding.hash-table | 1.18140052 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.1 / coding.hash-table | 10.747279 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.1 / coding.hash-table | 5.10115425 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.2 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.2 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.2 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.2 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.2 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.2 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.2 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.2 / coding.hash-table | 1.17763335 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.2 / coding.hash-table | 10.7095179 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.2 / coding.hash-table | 5.15377302 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | CHARACTERIZED | Three uninstrumented isolated native-v25 requests; not independent checkpoint quality or installed-product qualification;  |
| deployment-performance | CHARACTERIZED | Three uninstrumented isolated native-v25 requests; not independent checkpoint quality or installed-product qualification;  |
| product-path | CHARACTERIZED | Three uninstrumented isolated native-v25 requests; not independent checkpoint quality or installed-product qualification;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 1af3500d0025db9b262b42013b3105c5dc0d6fb49276ecb9c86fe5d484d3ad93 |
| representation | goal-v1-mxfp4-routed-q2_k; physical variant 572c0689fdc28e093656ba6986f18d1410a85891c690c3acad98f0d9269c4b8a |
| artifact_set | 25d29155aa86505ce0aa5a1c5535f9ed6e3b073d1803409a646b1f643423f041 |
| binding | 44c8bf1dc9cd3275f840aaf06efbab70dadf0f72a24af92b17aa60c14b31c236 |
| specialization | 92dec75ee03479cba2905305a424f0dd9bba421b1fa5695bf46f58033e898d85 |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | b8eba206553630f77bdfddc80b85051e28e566580c16f86b07cf7f8a66fc62c2 |
| build | 3161e5573f566279f257007362cd56f971a96ce26384678437b5e311f2d995ee |
| executable | efad2cb2738c76e3b7b656ffff18f417a6c46074bae2113cdca94397b87246ed |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; online64 executable realization |
| kernel_bundle | 2e0d07ff17219fa290bc4de8348c8769c2cf8de93ec6b083fc309df9544e75a3 |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated resident host; fresh session per sample after separate warmup; speculative; no prefix reuse |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | d5d9eb417abe63fc2d2c242317479cfc785ce46688e25cc7e936501111393f9d |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples; median, range and MAD, not a confidence interval or universal speedup.
- Same immutable artifact; changed attention numerical class, binding, specialization, build and kernels. Not an equivalent-realization A/B.
- No independent checkpoint-matched NLL/PPL/KL or held-out quality. High/maximum, HTTP and installed-product performance not qualified.
- Client-visible fragment arrival is not PTY painting; server/client latency metrics have distinct boundaries.
- Advisory locks and sampled clear GPU process observations do not establish uninterrupted exclusivity; clocks uncontrolled.
- Ordered output fragments are observed, not required equal across distinct numerical realizations and not independent checkpoint evidence.
- 4K coding and 16K prefill admitted after authorized operator-model unload; no 32K candidate admission or installed rollout claim.
