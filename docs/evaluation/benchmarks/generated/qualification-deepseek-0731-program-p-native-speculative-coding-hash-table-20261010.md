<!-- docs:metadata
title: "0731 Q2_K native protocol: speculative / coding.hash-table / three post-warmup fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-speculative-coding-hash-table-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-speculative-coding-hash-table-20261010.json
-->

# 0731 Q2_K native protocol: speculative / coding.hash-table / three post-warmup fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-speculative-coding-hash-table-20261010.json)

Target identity: `a835506d0b4aea13e25c24cbdf86fcead4c1a5374fe938787f60e401d2492c88`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.582333 | s | 3 | 0.565739–0.600458 | 0.0165937 |
| request.client-complete / coding.hash-table/turn-0 | 23.4137 | s | 3 | 23.4115–23.4443 | 0.00220129 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.82082 | s | 3 | 1.78243–1.84929 | 0.0284726 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.467791 | s | 3 | 0.465911–0.472476 | 0.00187999 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0846843 | s | 3 | 0.0846673–0.0848264 | 1.70367e-05 |
| ttft.server / coding.hash-table/turn-0 | 1.22034 | s | 3 | 1.21659–1.26698 | 0.00374762 |
| final.first.server / coding.hash-table/turn-0 | 1.22034 | s | 3 | 1.21659–1.26698 | 0.00374762 |
| final.first.client / coding.hash-table/turn-0 | 1.82082 | s | 3 | 1.78243–1.84929 | 0.0284726 |
| final.phase-rate / coding.hash-table/turn-0 | 11.6073 | token/s | 3 | 11.5893–11.6113 | 0.00400102 |
| decode.post-first.committed / coding.hash-table/turn-0 | 11.8086 | token/s | 3 | 11.7888–11.811 | 0.0024292 |
| prefill.uncached / coding.hash-table/turn-0 | 40.6447 | token/s | 3 | 38.4517–40.9049 | 0.260172 |
| prefill.wall / coding.hash-table/turn-0 | 0.762706 | s | 3 | 0.757855–0.806207 | 0.00485113 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.02725386e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/memory-4096.json |
| post-load.pss / coding.hash-table | 1.02723609e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/memory-4096.json |
| post-load.pss_file / coding.hash-table | 1.02487192e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/memory-4096.json |
| post-load.pss_anon / coding.hash-table | 192180224 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/memory-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/memory-4096.json |
| load.server / coding.hash-table | 6.18992033 | s | One observed server load-request to ready interval; file pages already read by artifact authentication, not cold storage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/host.jsonl |
| native.prompt_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-0 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-1 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_cycles.sample-2 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-0 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-1 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_forwards.sample-2 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-0 / coding.hash-table | 250 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-1 / coding.hash-table | 250 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.proposed_tokens.sample-2 / coding.hash-table | 250 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-0 / coding.hash-table | 249 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-1 / coding.hash-table | 249 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.selected_verification_tokens.sample-2 / coding.hash-table | 249 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-0 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-1 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.target_verifications.sample-2 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-0 / coding.hash-table | 156 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-1 / coding.hash-table | 156 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.accepted_tokens.sample-2 / coding.hash-table | 156 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-0 / coding.hash-table | 93 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-1 / coding.hash-table | 93 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.rejected_tokens.sample-2 / coding.hash-table | 93 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-0 / coding.hash-table | 1 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-1 / coding.hash-table | 1 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.discarded_tokens.sample-2 / coding.hash-table | 1 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-0 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-1 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.correction_or_bonus_tokens.sample-2 / coding.hash-table | 50 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-0 / coding.hash-table | 3.12 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-1 / coding.hash-table | 3.12 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.mean_accepted_prefix.sample-2 / coding.hash-table | 3.12 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-0 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-1 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.maximum_accepted_prefix.sample-2 / coding.hash-table | 5 | count | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-0 / coding.hash-table | 1.47099494 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-1 / coding.hash-table | 1.47231124 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.draft_seconds.sample-2 / coding.hash-table | 1.45417005 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-0 / coding.hash-table | 14.2491975 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-1 / coding.hash-table | 14.2513539 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.verification_seconds.sample-2 / coding.hash-table | 14.2838946 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-0 / coding.hash-table | 5.95682877 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-1 / coding.hash-table | 5.94348066 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |
| speculation.commit_seconds.sample-2 / coding.hash-table | 5.97021464 | s | Server-authored speculative cycle observation; phase intervals are not an additive complete-turn partition; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/events.jsonl |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Native emission and pinned independent GGUF reader establish structure, not checkpoint-matched quality; Independent held-out comparison missing |
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
| physical_policy | 1e1d143a28c02e50a29deb8aa1123eb4d16866e8e2d348aa1d7081f1a9e18da2 |
| representation | goal-v1-q2_k; family-preserved roles unchanged |
| artifact_set | 7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f |
| binding | a4bb99c92e1f8d1fb61f7db3ed0fc5d7a900fcc87b7ecd6fd8c75c486a939737 |
| specialization | 22c47bdb6eebe27e01cb9e40571b52582324274365d4c266ef2034e3c7e5aa6e |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 34aeafbdb4800b7002cd8d464f13044d144e565096e2a70ec2bab659b1b9dab9 |
| build | df8ef064dcacc221bf01ea8f544caa4f1290d647566f2124a65131840736812e |
| executable | 412247b97bc36c8c17e209abccc9e2235415802044690945d29181ac8a9bc89c |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; shared row matrix candidate |
| kernel_bundle | 43eb78da7f8beb3edfda0ee3cf12e18cc17db96808cafaba4b7d6d192ee4cea1 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-speculative/4096-coding.hash-table/receipt.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three samples characterize this exact configuration; no confidence interval or global performance claim.
- Source/build/hardware facts joined from directly observed owned daemon and frozen capture; client and server identities remain separate.
- No independent 0731 representation-quality reference; no high/maximum reasoning or installed-service qualification in this record. Strategy is exact and not mixed with another lane.
- CPU admission, visible-client timing and server generation phases have separate clocks/denominators.
- Resource observations are sampled; locks do not prove uninterrupted exclusive hardware ownership.
- Clock and thermal state were not sampled throughout the baseline; candidate partial device-state sampling is separate, not proof of clock equivalence.
- Generation identity/order equivalence is tested separately against the exact baseline, not an upstream model oracle.
