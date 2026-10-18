<!-- docs:metadata
title: "0731 MXFP4/Q2 native q2-pipeline realization: speculative / coding.hash-table / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-pipeline-speculative-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-pipeline-speculative-coding-hash-table-20261012.json
-->

# 0731 MXFP4/Q2 native q2-pipeline realization: speculative / coding.hash-table / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-pipeline-speculative-coding-hash-table-20261012.json)

Target identity: `f84db5d85d7dbc363eebf9f3cc75e8f17cda88f4c43959237427b6ca1d5b6d81`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.535954 | s | 3 | 0.506189–0.543061 | 0.00710714 |
| request.client-complete / coding.hash-table/turn-0 | 18.4584 | s | 3 | 18.4543–18.4587 | 0.00024176 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.67612 | s | 3 | 1.65222–1.68115 | 0.00503246 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.429616 | s | 3 | 0.428119–0.439171 | 0.00149732 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0657944 | s | 3 | 0.0657921–0.0659042 | 2.37138e-06 |
| ttft.server / coding.hash-table/turn-0 | 1.1452 | s | 3 | 1.13305–1.14603 | 0.000829783 |
| final.first.server / coding.hash-table/turn-0 | 1.1452 | s | 3 | 1.13305–1.14603 | 0.000829783 |
| final.first.client / coding.hash-table/turn-0 | 1.67612 | s | 3 | 1.65222–1.68115 | 0.00503246 |
| final.phase-rate / coding.hash-table/turn-0 | 14.8848 | token/s | 3 | 14.8486–14.8863 | 0.00153393 |
| decode.post-first.committed / coding.hash-table/turn-0 | 15.199 | token/s | 3 | 15.1737–15.1995 | 0.000562224 |
| prefill.uncached / coding.hash-table/turn-0 | 43.4512 | token/s | 3 | 42.8652–43.6194 | 0.168149 |
| prefill.wall / coding.hash-table/turn-0 | 0.713443 | s | 3 | 0.710693–0.723198 | 0.00275026 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.04096313e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/memory-post-load-4096.json |
| post-load.pss / coding.hash-table | 1.04085947e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/memory-post-load-4096.json |
| post-load.pss_file / coding.hash-table | 1.03844939e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/memory-post-load-4096.json |
| post-load.pss_anon / coding.hash-table | 196771840 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/memory-post-load-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/memory-post-load-4096.json |
| load.client / coding.hash-table | 5.4877004 | s | One load command to ready; artifact authentication already read file pages; not cold-storage latency; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/load-4096.json |
| native.prompt_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.0 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.0 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.0 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.0 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.0 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.0 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.0 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.0 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.0 / coding.hash-table | 1.18919744 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.0 / coding.hash-table | 10.843992 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.0 / coding.hash-table | 4.83313984 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.1 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.1 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.1 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.1 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.1 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.1 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.1 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.1 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.1 / coding.hash-table | 1.18675865 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.1 / coding.hash-table | 10.8825502 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.1 / coding.hash-table | 4.84207349 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.2 / coding.hash-table | 215 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.2 / coding.hash-table | 211 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.2 / coding.hash-table | 170 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.2 / coding.hash-table | 41 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.2 / coding.hash-table | 4 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.2 / coding.hash-table | 43 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.2 / coding.hash-table | 3.95348837 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.2 / coding.hash-table | 5 | count | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.2 / coding.hash-table | 1.18376306 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.2 / coding.hash-table | 10.856274 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.2 / coding.hash-table | 4.82961815 | s | Observed cycle economics; durations need not partition complete-turn wall time; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl |

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
| binding | 091f933e11d8dd129e86d186f622c20f637ffe98b23d2d78fe118bd1a2199632 |
| specialization | 92dec75ee03479cba2905305a424f0dd9bba421b1fa5695bf46f58033e898d85 |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | 0c7c95cf3ee71b51ac56861940abff3040d17b8487d65b875cd4edb1f6128913 |
| build | aad663f4c196b49f52bca1b6e71160fdc66deb231ea782bf76d32a614638d666 |
| executable | 95ec1ba0d94b272d379db6a30c53f2067326c0701f235d8f64dbaf3db7cc7b2f |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; q2-pipeline executable realization |
| kernel_bundle | 17cbe4c8f5c2bbc394f083fb5f451abe68ff57030b9882fd94545bd99f7c29dc |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-pipeline-native-01/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples; median, range and MAD, not a confidence interval or universal speedup.
- Same artifact/binding/numerical class; implementation/build/kernel differ. Instrumented traces excluded from timing.
- No independent checkpoint-matched NLL/PPL/KL or held-out quality. High/maximum, HTTP and installed-product performance not qualified.
- Client-visible fragment arrival is not PTY painting; server/client latency metrics have distinct boundaries.
- Advisory locks and sampled clear GPU process observations do not establish uninterrupted exclusivity; clocks uncontrolled.
- Exact internal output-byte agreement does not independently qualify model behavior.
- 4K coding and 16K prefill admitted after authorized operator-model unload; no 32K candidate admission or installed rollout claim.
