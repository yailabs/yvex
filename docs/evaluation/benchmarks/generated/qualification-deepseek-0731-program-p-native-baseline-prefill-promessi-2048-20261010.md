<!-- docs:metadata
title: "0731 Q2_K native protocol: baseline / prefill.promessi-2048 / three post-warmup fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-baseline-prefill-promessi-2048-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-baseline-prefill-promessi-2048-20261010.json
-->

# 0731 Q2_K native protocol: baseline / prefill.promessi-2048 / three post-warmup fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-baseline-prefill-promessi-2048-20261010.json)

Target identity: `68e25053d76ba9ab98bf32dc91bef8a1ea97677b7fe0e75c59e62da16f256b83`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.517637 | s | 3 | 0.506266–0.530264 | 0.0113718 |
| request.client-complete / prefill.promessi-2048/turn-0 | 28.4271 | s | 3 | 28.3649–28.4958 | 0.0621577 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 26.6612 | s | 3 | 26.5951–26.7199 | 0.058722 |
| delivery.client-gap.maximum / prefill.promessi-2048/turn-0 | 0.124891 | s | 3 | 0.123519–0.124953 | 6.2027e-05 |
| delivery.client-gap.mean / prefill.promessi-2048/turn-0 | 0.117986 | s | 3 | 0.117724–0.118391 | 0.000262122 |
| ttft.server / prefill.promessi-2048/turn-0 | 26.1309 | s | 3 | 26.0888–26.2017 | 0.0420835 |
| final.first.server / prefill.promessi-2048/turn-0 | 26.1309 | s | 3 | 26.0888–26.2017 | 0.0420835 |
| final.first.client / prefill.promessi-2048/turn-0 | 26.6612 | s | 3 | 26.5951–26.7199 | 0.058722 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 8.44514 | token/s | 3 | 8.42689–8.46932 | 0.0182484 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 78.7487 | token/s | 3 | 78.5317–78.8808 | 0.132097 |
| prefill.wall / prefill.promessi-2048/turn-0 | 26.0068 | s | 3 | 25.9632–26.0786 | 0.043552 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / prefill.promessi-2048 | 1.03034077e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/memory-16384.json |
| post-load.pss / prefill.promessi-2048 | 1.03032292e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/memory-16384.json |
| post-load.pss_file / prefill.promessi-2048 | 1.0256754e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/memory-16384.json |
| post-load.pss_anon / prefill.promessi-2048 | 420515840 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/memory-16384.json |
| post-load.locked / prefill.promessi-2048 | 0 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/memory-16384.json |
| load.server / prefill.promessi-2048 | 5.31869875 | s | One observed server load-request to ready interval; file pages already read by artifact authentication, not cold storage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/host.jsonl |
| native.prompt_tokens / prefill.promessi-2048 | 2048 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/events.jsonl |
| native.reused_tokens / prefill.promessi-2048 | 0 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/events.jsonl |
| native.prefill_tokens / prefill.promessi-2048 | 2048 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/events.jsonl |
| native.generated_tokens / prefill.promessi-2048 | 16 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/events.jsonl |

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
| source_delta | 0954ac9646d5d4a98dd2e460e7656c995ef27e0b1ad9e31548d1c03a056601c4 |
| build | 43b7e711cd984715151d7841f65fbd8270cf0e00589862a5bfb0d58fc0d08421 |
| executable | 9e4f3ea7c4f9b1fb942774117bfa6df0e405989d3a680fadc5471f1425c3b291 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; shared row baseline |
| kernel_bundle | 4a21492217e459a8c6391d6dfd068d8154f704186249bbb64e67a5cba173a6ad |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated resident host; fresh session per sample after separate warmup; target-only; no prefix reuse |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | ba06670744e623b715778ff9f208f54da79ac0d9899184436ca4b7b229c04c1e |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-baseline-v3/16384-prefill.promessi-2048/receipt.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three samples characterize this exact configuration; no confidence interval or global performance claim.
- Source/build/hardware facts joined from directly observed owned daemon and frozen capture; client and server identities remain separate.
- No independent 0731 representation-quality reference; no DSpark, high/maximum reasoning or installed-service qualification in this record.
- CPU admission, visible-client timing and server generation phases have separate clocks/denominators.
- Resource observations are sampled; locks do not prove uninterrupted exclusive hardware ownership.
- Clock and thermal state were not sampled throughout the baseline; candidate partial device-state sampling is separate, not proof of clock equivalence.
- Generation identity/order equivalence is tested separately against the exact baseline, not an upstream model oracle.
