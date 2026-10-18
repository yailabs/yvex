<!-- docs:metadata
title: "0731 MXFP4/Q2 native paired-b2 realization: target-only / coding.hash-table / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-paired-b2-target-only-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-paired-b2-target-only-coding-hash-table-20261012.json
-->

# 0731 MXFP4/Q2 native paired-b2 realization: target-only / coding.hash-table / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-paired-b2-target-only-coding-hash-table-20261012.json)

Target identity: `2330bce37727405d9503db9d4697910d2a61d4e1b1d8abdefab3584430c777ec`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.412217 | s | 3 | 0.393598–0.44974 | 0.0186184 |
| request.client-complete / coding.hash-table/turn-0 | 26.8287 | s | 3 | 26.6485–26.8459 | 0.0171704 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.22917 | s | 3 | 1.2108–1.26487 | 0.0183711 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.114391 | s | 3 | 0.113433–0.114444 | 5.283e-05 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.100315 | s | 3 | 0.0997539–0.100388 | 7.28982e-05 |
| ttft.server / coding.hash-table/turn-0 | 0.816947 | s | 3 | 0.815118–0.817174 | 0.000227503 |
| final.first.server / coding.hash-table/turn-0 | 0.816947 | s | 3 | 0.815118–0.817174 | 0.000227503 |
| final.first.client / coding.hash-table/turn-0 | 1.22917 | s | 3 | 1.2108–1.26487 | 0.0183711 |
| final.phase-rate / coding.hash-table/turn-0 | 9.96485 | token/s | 3 | 9.95775–10.0211 | 0.00709933 |
| decode.post-first.committed / coding.hash-table/turn-0 | 9.96859 | token/s | 3 | 9.96135–10.0246 | 0.00723879 |
| prefill.uncached / coding.hash-table/turn-0 | 43.8387 | token/s | 3 | 43.7721–43.9692 | 0.0665451 |
| prefill.wall / coding.hash-table/turn-0 | 0.707138 | s | 3 | 0.70504–0.708213 | 0.00107504 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / coding.hash-table | 1.04094929e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/memory-post-load-4096.json |
| post-load.pss / coding.hash-table | 1.04089543e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/memory-post-load-4096.json |
| post-load.pss_file / coding.hash-table | 1.03849697e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/memory-post-load-4096.json |
| post-load.pss_anon / coding.hash-table | 195608576 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/memory-post-load-4096.json |
| post-load.locked / coding.hash-table | 0 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/memory-post-load-4096.json |
| load.client / coding.hash-table | 5.38706727 | s | One load command to ready; artifact authentication already read file pages; not cold-storage latency; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/load-4096.json |
| native.prompt_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl |
| native.reused_tokens / coding.hash-table | 0 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl |
| native.prefill_tokens / coding.hash-table | 31 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl |
| native.generated_tokens / coding.hash-table | 256 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl |

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
| source_delta | d0e382eadc6494d163710a2266f2900889967dfa12e47860b448a195fe388327 |
| build | 60188e34a64a9d9e7bf5a484e022d8d00ad542931cf3edde9c5e39649d9e4d37 |
| executable | 80d6f793aa9ac828a10eeffba2592b7b28574633495ba54df26f2660dec0fcbb |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; attention executable realization |
| kernel_bundle | 9d33269d8e4fc2808aad3801f6972e5df4f3cb42af597aa22bbbcf4824c45c51 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/paired-attention-native-01/B2-target-only/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples; median, range and MAD, not a confidence interval or universal speedup.
- Same artifact/binding/numerical class; implementation/build/kernel differ. Instrumented traces excluded from timing.
- No independent checkpoint-matched NLL/PPL/KL or held-out quality. High/maximum, HTTP and installed-product performance not qualified.
- Client-visible fragment arrival is not PTY painting; server/client latency metrics have distinct boundaries.
- Advisory locks and sampled clear GPU process observations do not establish uninterrupted exclusivity; clocks uncontrolled.
- Exact internal output-byte agreement does not independently qualify model behavior.
- 4K coding and 16K prefill admitted after authorized operator-model unload; no 32K candidate admission or installed rollout claim.
