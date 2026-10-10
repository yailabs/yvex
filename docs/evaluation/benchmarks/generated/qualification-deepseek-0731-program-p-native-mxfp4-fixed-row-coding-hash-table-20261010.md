<!-- docs:metadata
title: "0731 MXFP4 / routed Q2_K native protocol: mxfp4-fixed-row / coding.hash-table / three post-warmup fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-mxfp4-fixed-row-coding-hash-table-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-mxfp4-fixed-row-coding-hash-table-20261010.json
-->

# 0731 MXFP4 / routed Q2_K native protocol: mxfp4-fixed-row / coding.hash-table / three post-warmup fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-mxfp4-fixed-row-coding-hash-table-20261010.json)

Target identity: `0c45aa2028dcf841c5233aac47bfcd92a258381ee7122b4ccf1733e1af42ffed`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.49889 | s | 3 | 0.489955–0.512392 | 0.0089352 |
| request.client-complete / coding.hash-table/turn-0 | 27.7882 | s | 3 | 27.7842–27.8114 | 0.00402461 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.39383 | s | 3 | 1.38726–1.40644 | 0.00656627 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.117539 | s | 3 | 0.117265–0.11771 | 0.000170801 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.103516 | s | 3 | 0.103456–0.103597 | 5.93964e-05 |
| ttft.server / coding.hash-table/turn-0 | 0.894902 | s | 3 | 0.894022–0.897258 | 0.000879955 |
| final.first.server / coding.hash-table/turn-0 | 0.894902 | s | 3 | 0.894022–0.897258 | 0.000879955 |
| final.first.client / coding.hash-table/turn-0 | 1.39383 | s | 3 | 1.38726–1.40644 | 0.00656627 |
| final.phase-rate / coding.hash-table/turn-0 | 9.65738 | token/s | 3 | 9.64958–9.66314 | 0.00576213 |
| decode.post-first.committed / coding.hash-table/turn-0 | 9.66035 | token/s | 3 | 9.6528–9.6659 | 0.0055481 |
| prefill.uncached / coding.hash-table/turn-0 | 39.5972 | token/s | 3 | 39.4666–39.624 | 0.0268003 |
| prefill.wall / coding.hash-table/turn-0 | 0.782884 | s | 3 | 0.782354–0.785474 | 0.000529515 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.03756243e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/memory-4096.json |
| post-load.pss / coding.hash-table | 1.03754457e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/memory-4096.json |
| post-load.pss_file / coding.hash-table | 1.03518487e+11 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/memory-4096.json |
| post-load.pss_anon / coding.hash-table | 191733760 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/memory-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup immediately post-load; overlapping accounting classes, not additive CUDA/device allocations; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/memory-4096.json |
| load.server / coding.hash-table | 8.81141739 | s | One observed server load-request to ready interval; file pages already read by artifact authentication, not cold storage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/host.jsonl |
| native.prompt_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server-authored count per completed request; identical in all three samples; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/events.jsonl |

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
| source_delta | 8aa4866662f7984d464c49d4f76c0a41cf3618ab1950f63d0c6f54aaf9e99d73 |
| build | 25e5893fce501226f3b08ef02eea388bd5101a6dbbc0fbb1a3e578476a442757 |
| executable | 4a9c41b1c29bd9020a61a6efe50fd2836be3399a1aaa94084154bb784c32f5dd |
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
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | d5d9eb417abe63fc2d2c242317479cfc785ce46688e25cc7e936501111393f9d |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 client to isolated resident host; controlled selected configuration, not installed-product defaults. Session: fresh; warm/cold: resident engine after explicit separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-mxfp4-native-fixed-row/4096-coding.hash-table/receipt.json.

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
