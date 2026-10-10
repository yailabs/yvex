<!-- docs:metadata
title: "0731 MXFP4 / routed Q2_K native protocol: mxfp4-specialized-row / prefill.promessi-8192 / three post-warmup fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-mxfp4-specialized-row-prefill-promessi-8192-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-mxfp4-specialized-row-prefill-promessi-8192-20261010.json
-->

# 0731 MXFP4 / routed Q2_K native protocol: mxfp4-specialized-row / prefill.promessi-8192 / three post-warmup fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-mxfp4-specialized-row-prefill-promessi-8192-20261010.json)

Target identity: `b6e4741d1a5449c7c09d93423b0f1f9ce0d82a3b4e160814cdf9e6eb9534ec0c`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.525188 | s | 3 | 0.514697–0.528696 | 0.00350856 |
| request.client-complete / prefill.promessi-8192/turn-0 | 94.0788 | s | 3 | 93.9646–94.2274 | 0.114159 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 92.2603 | s | 3 | 92.1486–92.4005 | 0.111654 |
| delivery.client-gap.maximum / prefill.promessi-8192/turn-0 | 0.128507 | s | 3 | 0.127291–0.128747 | 0.00024072 |
| delivery.client-gap.mean / prefill.promessi-8192/turn-0 | 0.121228 | s | 3 | 0.121061–0.121782 | 0.00016743 |
| ttft.server / prefill.promessi-8192/turn-0 | 91.7455 | s | 3 | 91.6199–91.8753 | 0.125656 |
| final.first.server / prefill.promessi-8192/turn-0 | 91.7455 | s | 3 | 91.6199–91.8753 | 0.125656 |
| final.first.client / prefill.promessi-8192/turn-0 | 92.2603 | s | 3 | 92.1486–92.4005 | 0.111654 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 8.20288 | token/s | 3 | 8.19016–8.2378 | 0.0127204 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 89.4134 | token/s | 3 | 89.2898–89.5467 | 0.123595 |
| prefill.wall / prefill.promessi-8192/turn-0 | 91.6194 | s | 3 | 91.483–91.7462 | 0.126819 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / prefill.promessi-8192 | 1.04358588e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/memory-16384.json |
| post-load.pss / prefill.promessi-8192 | 1.04356803e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/memory-16384.json |
| post-load.pss_file / prefill.promessi-8192 | 1.03851958e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/memory-16384.json |
| post-load.pss_anon / prefill.promessi-8192 | 460607488 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/memory-16384.json |
| post-load.locked / prefill.promessi-8192 | 0 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/memory-16384.json |
| load.server / prefill.promessi-8192 | 5.76124353 | s | One observed server load-request to ready interval; file pages already read by artifact authentication, not cold storage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/host.jsonl |
| native.prompt_tokens / prefill.promessi-8192 | 8192 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/events.jsonl |
| native.reused_tokens / prefill.promessi-8192 | 0 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/events.jsonl |
| native.prefill_tokens / prefill.promessi-8192 | 8192 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/events.jsonl |
| native.generated_tokens / prefill.promessi-8192 | 16 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/events.jsonl |

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
| source_delta | d5a0ea8d21c57f3c4ea579ae0ac11dd540a2b54e0873ebc110b66355ea6c93f7 |
| build | 280ad26711d7bed7143dc3af18bcc6cfedc14ef8a44f02710a02a5a0b242eb08 |
| executable | 2f036337cb35b634e964ca03fe21b578d8366c1d758301c33ae0b21ae7aa7ae4 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted candidate |
| kernel_bundle | NOT RETAINED |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-specialized-row-v2/16384-prefill.promessi-8192/receipt.json.

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
- Rejected implementation candidate; published observations do not recommend this executable.
