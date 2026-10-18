<!-- docs:metadata
title: "0731 MXFP4/Q2 native online64 realization: target-only / prefill.promessi-2048 / three fresh sessions"
id: yvex.evaluation.qualification.deepseek-0731-program-p-online64-target-only-prefill-promessi-2048-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-online64-target-only-prefill-promessi-2048-20261012.json
-->

# 0731 MXFP4/Q2 native online64 realization: target-only / prefill.promessi-2048 / three fresh sessions

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-online64-target-only-prefill-promessi-2048-20261012.json)

Target identity: `b1097f13c7e9aac79e135367f591b5ca7f125bdb993919c25abf035d2a965d76`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.587536 | s | 3 | 0.583701–0.615749 | 0.0038351 |
| request.client-complete / prefill.promessi-2048/turn-0 | 15.4112 | s | 3 | 15.3977–15.4531 | 0.0135772 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 13.765 | s | 3 | 13.7549–13.8064 | 0.0100261 |
| delivery.client-gap.maximum / prefill.promessi-2048/turn-0 | 0.11749 | s | 3 | 0.116627–0.120328 | 0.000863026 |
| delivery.client-gap.mean / prefill.promessi-2048/turn-0 | 0.109744 | s | 3 | 0.109507–0.109775 | 3.08504e-05 |
| ttft.server / prefill.promessi-2048/turn-0 | 13.1812 | s | 3 | 13.1674–13.1906 | 0.00937766 |
| final.first.server / prefill.promessi-2048/turn-0 | 13.1812 | s | 3 | 13.1674–13.1906 | 0.00937766 |
| final.first.client / prefill.promessi-2048/turn-0 | 13.765 | s | 3 | 13.7549–13.8064 | 0.0100261 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 9.08582 | token/s | 3 | 9.08385–9.10532 | 0.0019693 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 156.752 | token/s | 3 | 156.629–156.911 | 0.122623 |
| prefill.wall / prefill.promessi-2048/turn-0 | 13.0653 | s | 3 | 13.052–13.0755 | 0.0102286 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| post-load.rss / prefill.promessi-2048 | 1.04450109e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/memory-post-load-16384.json |
| post-load.pss / prefill.promessi-2048 | 1.04440415e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/memory-post-load-16384.json |
| post-load.pss_file / prefill.promessi-2048 | 1.0384609e+11 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/memory-post-load-16384.json |
| post-load.pss_anon / prefill.promessi-2048 | 550088704 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/memory-post-load-16384.json |
| post-load.locked / prefill.promessi-2048 | 0 | byte | OS smaps_rollup: overlapping classes, not additive allocations or proof of CUDA pinning; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/memory-post-load-16384.json |
| load.client / prefill.promessi-2048 | 5.54517564 | s | One load command to ready; artifact authentication already read file pages; not cold-storage latency; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/load-16384.json |
| native.prompt_tokens / prefill.promessi-2048 | 2048 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl |
| native.reused_tokens / prefill.promessi-2048 | 0 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl |
| native.prefill_tokens / prefill.promessi-2048 | 2048 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl |
| native.generated_tokens / prefill.promessi-2048 | 16 | count | Server population per completed request; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl |

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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-native-03/target-only/16384-prefill.promessi-2048/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples; median, range and MAD, not a confidence interval or universal speedup.
- Same immutable artifact; changed attention numerical class, binding, specialization, build and kernels. Not an equivalent-realization A/B.
- No independent checkpoint-matched NLL/PPL/KL or held-out quality. High/maximum, HTTP and installed-product performance not qualified.
- Client-visible fragment arrival is not PTY painting; server/client latency metrics have distinct boundaries.
- Advisory locks and sampled clear GPU process observations do not establish uninterrupted exclusivity; clocks uncontrolled.
- Ordered output fragments are observed, not required equal across distinct numerical realizations and not independent checkpoint evidence.
- 4K coding and 16K prefill admitted after authorized operator-model unload; no 32K candidate admission or installed rollout claim.
