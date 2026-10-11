<!-- docs:metadata
title: "0731 MXFP4 / routed Q2_K native mainline: speculative / coding.hash-table / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mainline-speculative-coding-hash-table-20261011
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mainline-speculative-coding-hash-table-20261011.json
-->

# 0731 MXFP4 / routed Q2_K native mainline: speculative / coding.hash-table / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mainline-speculative-coding-hash-table-20261011.json)

Target identity: `4d73a1efdc3b91a14c4317913833637c42e6f15f181d534ae74c2947a504d2a4`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.575754 | s | 3 | 0.565289–0.58456 | 0.00880578 |
| request.client-complete / coding.hash-table/turn-0 | 19.5807 | s | 3 | 19.576–19.5909 | 0.00471326 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.80941 | s | 3 | 1.80634–1.83973 | 0.00307725 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.462616 | s | 3 | 0.456846–0.464698 | 0.00208266 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0696826 | s | 3 | 0.0696104–0.0696893 | 6.67149e-06 |
| ttft.server / coding.hash-table/turn-0 | 1.24105 | s | 3 | 1.23365–1.25518 | 0.00740288 |
| final.first.server / coding.hash-table/turn-0 | 1.24105 | s | 3 | 1.23365–1.25518 | 0.00740288 |
| final.first.client / coding.hash-table/turn-0 | 1.80941 | s | 3 | 1.80634–1.83973 | 0.00307725 |
| final.phase-rate / coding.hash-table/turn-0 | 14.0561 | token/s | 3 | 14.0549–14.0654 | 0.00123128 |
| decode.post-first.committed / coding.hash-table/turn-0 | 14.3508 | token/s | 3 | 14.3495–14.3658 | 0.00130921 |
| prefill.uncached / coding.hash-table/turn-0 | 38.8842 | token/s | 3 | 38.5119–39.2473 | 0.36317 |
| prefill.wall / coding.hash-table/turn-0 | 0.79724 | s | 3 | 0.789862–0.804946 | 0.00737715 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.04093819e+11 | byte | OS smaps_rollup post-load; overlapping classes, not additive CUDA allocations or proof of pinning; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/memory-post-load-4096.json |
| post-load.pss / coding.hash-table | 1.04083017e+11 | byte | OS smaps_rollup post-load; overlapping classes, not additive CUDA allocations or proof of pinning; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/memory-post-load-4096.json |
| post-load.pss_file / coding.hash-table | 1.03843233e+11 | byte | OS smaps_rollup post-load; overlapping classes, not additive CUDA allocations or proof of pinning; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/memory-post-load-4096.json |
| post-load.pss_anon / coding.hash-table | 195547136 | byte | OS smaps_rollup post-load; overlapping classes, not additive CUDA allocations or proof of pinning; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/memory-post-load-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup post-load; overlapping classes, not additive CUDA allocations or proof of pinning; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/memory-post-load-4096.json |
| load.client / coding.hash-table | 5.51430754 | s | One native load command to ready, including connection; artifact authentication already read file pages, not cold storage; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/load-4096.json |
| native.prompt_tokens / coding.hash-table | 31 | count | Server count per completed request; identical in three samples; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server count per completed request; identical in three samples; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server count per completed request; identical in three samples; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server count per completed request; identical in three samples; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.0 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.0 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.0 / coding.hash-table | 215 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.0 / coding.hash-table | 211 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.0 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.0 / coding.hash-table | 170 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.0 / coding.hash-table | 41 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.0 / coding.hash-table | 4 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.0 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.0 / coding.hash-table | 3.95348837 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.0 / coding.hash-table | 5 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.0 / coding.hash-table | 1.24818038 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.0 / coding.hash-table | 11.6001571 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.0 / coding.hash-table | 5.02807579 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.1 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.1 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.1 / coding.hash-table | 215 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.1 / coding.hash-table | 211 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.1 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.1 / coding.hash-table | 170 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.1 / coding.hash-table | 41 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.1 / coding.hash-table | 4 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.1 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.1 / coding.hash-table | 3.95348837 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.1 / coding.hash-table | 5 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.1 / coding.hash-table | 1.25235799 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.1 / coding.hash-table | 11.6020219 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.1 / coding.hash-table | 5.02872789 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.2 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.2 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.2 / coding.hash-table | 215 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.2 / coding.hash-table | 211 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.2 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.2 / coding.hash-table | 170 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.2 / coding.hash-table | 41 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.2 / coding.hash-table | 4 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.2 / coding.hash-table | 43 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.2 / coding.hash-table | 3.95348837 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.2 / coding.hash-table | 5 | count | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.2 / coding.hash-table | 1.24967456 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.2 / coding.hash-table | 11.5978593 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.2 / coding.hash-table | 5.04126656 | s | Server speculative cycle observation; intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | CHARACTERIZED | Exact isolated native-v25 configuration; three deterministic completed requests, not installed-service or independent quality qualification;  |
| deployment-performance | CHARACTERIZED | Exact isolated native-v25 configuration; three deterministic completed requests, not installed-service or independent quality qualification;  |
| product-path | CHARACTERIZED | Exact isolated native-v25 configuration; three deterministic completed requests, not installed-service or independent quality qualification;  |

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
| source_commit | 3c73c2a5a295cb8c3022effc4395441a75cd8f3c |
| source_tree | 7fcc485fbaba0f5fb7d72e472d8e7ed2fc8a354d |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | 16a312112794861fc6e444b68e26f1a14b4fd88e189af533af9bcef4ef1b01fa |
| executable | 5ea780c3f3c6b07f1822944059a03ae56b3190ad80537f41b68340eed5dc6c8e |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted candidate |
| kernel_bundle | fe4eac599c25a7a6fbb33b904383569c73993118496fc264e6f24c6a2aaf13f2 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/attempt-3/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three uninstrumented samples; no confidence interval, controlled-clock equivalence or speedup claim.
- Source/build/executable and kernel digest authenticated from the frozen same executable. Profiled diagnostic times are excluded.
- No independent checkpoint-matched representation quality; high/maximum reasoning, HTTP and installed-product performance unqualified here.
- Client content arrival is not PTY painting; server and client metrics have different denominators.
- Sampled clear GPU-process lists and advisory locks do not prove uninterrupted exclusivity.
- All ordered fragment channels, byte lengths and SHA-256 values equal the retained same-artifact control; not independent numerical evidence.
- Successful 4K/16K admission follows authorized old-engine unload, not a weaker reserve or evidence for 32K.
- Retained historical MXFP4 native records lack a kernel bundle digest: descriptive context only; strict automatic cross-build comparison must refuse, not fill missing identity by inference.
