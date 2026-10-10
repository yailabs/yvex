<!-- docs:metadata
title: "0731 MXFP4 / routed Q2_K native protocol: mxfp4-speculative / coding.hash-table / three post-warmup fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-mxfp4-speculative-coding-hash-table-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-mxfp4-speculative-coding-hash-table-20261010.json
-->

# 0731 MXFP4 / routed Q2_K native protocol: mxfp4-speculative / coding.hash-table / three post-warmup fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-mxfp4-speculative-coding-hash-table-20261010.json)

Target identity: `304177388665cb2b7d655c36f1d638667e481029dd475d0185d07ac97644ee5e`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.564318 | s | 3 | 0.55175–0.590682 | 0.0125682 |
| request.client-complete / coding.hash-table/turn-0 | 19.3495 | s | 3 | 19.2994–19.3523 | 0.00283322 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.78882 | s | 3 | 1.77465–1.80013 | 0.0113049 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.447966 | s | 3 | 0.445432–0.45426 | 0.002534 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0688308 | s | 3 | 0.0687232–0.0688641 | 3.32619e-05 |
| ttft.server / coding.hash-table/turn-0 | 1.2229 | s | 3 | 1.20944–1.2245 | 0.00159681 |
| final.first.server / coding.hash-table/turn-0 | 1.2229 | s | 3 | 1.20944–1.2245 | 0.00159681 |
| final.first.client / coding.hash-table/turn-0 | 1.78882 | s | 3 | 1.77465–1.80013 | 0.0113049 |
| final.phase-rate / coding.hash-table/turn-0 | 14.2297 | token/s | 3 | 14.2221–14.2514 | 0.00760161 |
| decode.post-first.committed / coding.hash-table/turn-0 | 14.5285 | token/s | 3 | 14.5215–14.5513 | 0.00701778 |
| prefill.uncached / coding.hash-table/turn-0 | 39.5461 | token/s | 3 | 39.5163–40.2297 | 0.0297706 |
| prefill.wall / coding.hash-table/turn-0 | 0.783895 | s | 3 | 0.770574–0.784486 | 0.000590567 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.04089874e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/memory-4096.json |
| post-load.pss / coding.hash-table | 1.04088089e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/memory-4096.json |
| post-load.pss_file / coding.hash-table | 1.03851389e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/memory-4096.json |
| post-load.pss_anon / coding.hash-table | 192462848 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/memory-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/memory-4096.json |
| load.server / coding.hash-table | 6.70226783 | s | One observed server load-request to ready interval; file pages already read by artifact authentication, not cold storage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/host.jsonl |
| native.prompt_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-0 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-1 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-2 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-0 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-1 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-2 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-0 / coding.hash-table | 215 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-1 / coding.hash-table | 215 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-2 / coding.hash-table | 215 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-0 / coding.hash-table | 211 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-1 / coding.hash-table | 211 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-2 / coding.hash-table | 211 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-0 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-1 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-2 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-0 / coding.hash-table | 170 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-1 / coding.hash-table | 170 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-2 / coding.hash-table | 170 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-0 / coding.hash-table | 41 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-1 / coding.hash-table | 41 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-2 / coding.hash-table | 41 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-0 / coding.hash-table | 4 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-1 / coding.hash-table | 4 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-2 / coding.hash-table | 4 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-0 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-1 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-2 / coding.hash-table | 43 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-0 / coding.hash-table | 3.95348837 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-1 / coding.hash-table | 3.95348837 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-2 / coding.hash-table | 3.95348837 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-0 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-1 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-2 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-0 / coding.hash-table | 1.22314278 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-1 / coding.hash-table | 1.23676082 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-2 / coding.hash-table | 1.23960193 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-0 / coding.hash-table | 11.5216853 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-1 / coding.hash-table | 11.487262 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-2 / coding.hash-table | 11.4976376 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-0 / coding.hash-table | 4.92065963 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-1 / coding.hash-table | 4.91593524 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-2 / coding.hash-table | 4.9361878 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/events.jsonl |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | CHARACTERIZED | Exact isolated native-v25 target; three completed deterministic requests, not installed-service defaults or independent model quality;  |
| deployment-performance | CHARACTERIZED | Exact isolated native-v25 target; three completed deterministic requests, not installed-service defaults or independent model quality;  |
| product-path | CHARACTERIZED | Exact isolated native-v25 target; three completed deterministic requests, not installed-service defaults or independent model quality;  |

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
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 516b569f2204993ac54a4a1a61535b8cf26781ede78fe8d9a7b8995abd4d1072 |
| build | 407b1df74781f1d78b0459f94f874475ce2ae5667552e668646524466ff16a1e |
| executable | 4507f4f3ed6a4e10e92c04b6b2975c189ea48f4630d51f33b789f15aaa98862a |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted candidate |
| kernel_bundle | NOT RETAINED |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-q2-native-speculative/4096-coding.hash-table/receipt.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three samples characterize this exact configuration; no confidence interval or global performance claim.
- Source/build/hardware facts joined from directly observed owned daemon and frozen capture; client and server identities remain separate.
- No independent 0731 representation-quality reference; no high/maximum reasoning or installed-service qualification in this record. Strategy is exact and not mixed with another lane.
- CPU admission, visible-client timing and server generation phases have separate clocks/denominators.
- Resource observations are sampled; locks do not prove uninterrupted exclusive hardware ownership.
- Clock and thermal state were not sampled throughout the baseline; candidate partial device-state sampling is separate, not proof of clock equivalence.
- Physical quantization differs from Q2 control; no bitwise continuation equivalence or quality superiority is asserted.
- Kernel bundle digest is not projected in this native receipt; source/build/executable and specialization are retained independently.
