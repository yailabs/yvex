<!-- docs:metadata
title: "0731 MXFP4/Q2 native word-staging realization: target-only / prefill.promessi-8192 / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-word-staging-target-only-prefill-promessi-8192-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-word-staging-target-only-prefill-promessi-8192-20261012.json
-->

# 0731 MXFP4/Q2 native word-staging realization: target-only / prefill.promessi-8192 / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-word-staging-target-only-prefill-promessi-8192-20261012.json)

Target identity: `5ae7210b37049487874622cdc516765bbbb8cc673109cc40f130fe044f53df2c`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.511652 | s | 3 | 0.505573–0.538363 | 0.00607936 |
| request.client-complete / prefill.promessi-8192/turn-0 | 68.6851 | s | 3 | 68.6116–68.8084 | 0.0734763 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 67.005 | s | 3 | 66.9288–67.1315 | 0.0762643 |
| delivery.client-gap.maximum / prefill.promessi-8192/turn-0 | 0.121352 | s | 3 | 0.117829–0.124189 | 0.00283679 |
| delivery.client-gap.mean / prefill.promessi-8192/turn-0 | 0.111994 | s | 3 | 0.111792–0.11218 | 0.000186063 |
| ttft.server / prefill.promessi-8192/turn-0 | 66.4666 | s | 3 | 66.4171–66.6258 | 0.0495144 |
| final.first.server / prefill.promessi-8192/turn-0 | 66.4666 | s | 3 | 66.4171–66.6258 | 0.0495144 |
| final.first.client / prefill.promessi-8192/turn-0 | 67.005 | s | 3 | 66.9288–67.1315 | 0.0762643 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 8.91483 | token/s | 3 | 8.891–8.92938 | 0.0145476 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 123.468 | token/s | 3 | 123.172–123.564 | 0.0959753 |
| prefill.wall / prefill.promessi-8192/turn-0 | 66.3494 | s | 3 | 66.2979–66.5085 | 0.0515354 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / prefill.promessi-8192 | 1.04211165e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/memory-post-load-16384.json |
| post-load.pss / prefill.promessi-8192 | 1.04200741e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/memory-post-load-16384.json |
| post-load.pss_file / prefill.promessi-8192 | 1.03696888e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/memory-post-load-16384.json |
| post-load.pss_anon / prefill.promessi-8192 | 459616256 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/memory-post-load-16384.json |
| post-load.locked / prefill.promessi-8192 | 0 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/memory-post-load-16384.json |
| load.client / prefill.promessi-8192 | 6.85457807 | s | One load command to ready; artifact authentication already read file pages; not cold-storage latency; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/load-16384.json |
| native.prompt_tokens / prefill.promessi-8192 | 8192 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl |
| native.reused_tokens / prefill.promessi-8192 | 0 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl |
| native.prefill_tokens / prefill.promessi-8192 | 8192 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl |
| native.generated_tokens / prefill.promessi-8192 | 16 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl |

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
| source_delta | 911cc8a772365e6272e83260d7e623449cf7dd71760467e691b656b20a194e1c |
| build | fb89bd20d3fd748f64ffd4c8bc8e1ab1a5e4065011ff52046f84edd906fd2f93 |
| executable | 50e854ac4b7d618049072d34129c3db157adc549e70e67efec9aa083cfae3576 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; word-staging executable realization |
| kernel_bundle | 0743fb8f1dd6d79d01f459391b2ca626d14e6cdfedb0642c0c5cec29a28bf2ad |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/word-staging-native-01/target-only/16384-prefill.promessi-8192/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples; median, range and MAD, not a confidence interval or universal speedup.
- Same artifact/binding/numerical class; implementation/build/kernel differ. Instrumented traces excluded from timing.
- No independent checkpoint-matched NLL/PPL/KL or held-out quality. High/maximum, HTTP and installed-product performance not qualified.
- Client-visible fragment arrival is not PTY painting; server/client latency metrics have distinct boundaries.
- Advisory locks and sampled clear GPU process observations do not establish uninterrupted exclusivity; clocks uncontrolled.
- Exact internal output-byte agreement does not independently qualify model behavior.
- 4K coding and 16K prefill admitted after authorized operator-model unload; no 32K candidate admission or installed rollout claim.
