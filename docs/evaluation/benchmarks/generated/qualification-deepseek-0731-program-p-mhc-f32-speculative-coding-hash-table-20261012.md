<!-- docs:metadata
title: "0731 target mHC F32 accumulation: mhc-f32 / speculative / coding.hash-table"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mhc-f32-speculative-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mhc-f32-speculative-coding-hash-table-20261012.json
-->

# 0731 target mHC F32 accumulation: mhc-f32 / speculative / coding.hash-table

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mhc-f32-speculative-coding-hash-table-20261012.json)

Target identity: `4c72c469f61b2942d0008e12d9bf043dd7d0f7a975f7136b4c14849d6c00aff0`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.545161 | s | 3 | 0.536588–0.574199 | 0.00857337 |
| request.client-complete / coding.hash-table/turn-0 | 18.3171 | s | 3 | 18.2872–18.3339 | 0.0167597 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.58525 | s | 3 | 1.57476–1.591 | 0.00575379 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.433221 | s | 3 | 0.429163–0.434971 | 0.00175009 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0656137 | s | 3 | 0.0655377–0.0656569 | 4.3196e-05 |
| ttft.server / coding.hash-table/turn-0 | 1.03817 | s | 3 | 1.01678–1.04006 | 0.00189391 |
| final.first.server / coding.hash-table/turn-0 | 1.03817 | s | 3 | 1.01678–1.04006 | 0.00189391 |
| final.first.client / coding.hash-table/turn-0 | 1.58525 | s | 3 | 1.57476–1.591 | 0.00575379 |
| final.phase-rate / coding.hash-table/turn-0 | 14.9055 | token/s | 3 | 14.8981–14.9343 | 0.0073471 |
| decode.post-first.committed / coding.hash-table/turn-0 | 15.2412 | token/s | 3 | 15.2312–15.2589 | 0.010008 |
| prefill.uncached / coding.hash-table/turn-0 | 52.7593 | token/s | 3 | 50.9974–53.1041 | 0.3448 |
| prefill.wall / coding.hash-table/turn-0 | 0.587574 | s | 3 | 0.583759–0.607874 | 0.00381506 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.hash-table/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 31 | token | 3 | 31–31 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 31 | token | 3 | 31–31 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 42 | cycle | 3 | 42–42 | 0 |
| Draft forwards | 42 | forward | 3 | 42–42 | 0 |
| Proposed | 210 | token | 3 | 210–210 | 0 |
| Selected verification | 209 | token | 3 | 209–209 | 0 |
| Target verifications | 42 | verification | 3 | 42–42 | 0 |
| Accepted draft | 172 | token | 3 | 172–172 | 0 |
| Rejected draft | 37 | token | 3 | 37–37 | 0 |
| Discarded draft | 1 | token | 3 | 1–1 | 0 |
| Correction/bonus | 42 | token | 3 | 42–42 | 0 |
| Per-sample mean accepted prefix | 4.09524 | token | 3 | 4.09524–4.09524 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 1.15488 | s | 3 | 1.1526–1.16506 | 0.00227714 |
| Verification phase | 10.5712 | s | 3 | 10.5146–10.5872 | 0.0159928 |
| Speculative commit phase | 5.11863 | s | 3 | 5.11768–5.15244 | 0.000943568 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | Three isolated native requests; distinct numerical realization, no installed rollout or independent checkpoint quality;  |
| checkpoint-reference | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| deployment-performance | CHARACTERIZED | Three isolated native requests; distinct numerical realization, no installed rollout or independent checkpoint quality;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three isolated native requests; distinct numerical realization, no installed rollout or independent checkpoint quality;  |
| representation-quality | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |

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
| binding | 419aba8bad1bfc8432501ee75b2cea990b59eee82bd95fd4991240e35151d570 |
| specialization | 92dec75ee03479cba2905305a424f0dd9bba421b1fa5695bf46f58033e898d85 |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | 70dfe9997811c63e9376a8e70f6c88e19f9bbd35fcb62f07f54e72acf927beaa |
| build | fc0ecef0958e2e2ed9efad7f70f433c644b8bb6b3133af13a8bd00944a5ec756 |
| executable | 7f3083fdbece2c3ad36b60ab3c3221e8fd96ae927ed0012444727671191376be |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; mhc-f32 executable realization |
| kernel_bundle | 6c90c825be8fed9d42be1109dafa444c48409e00e67bb379d055dd5421f81d93 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/mhc-f32-native-01/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples, not a confidence interval.
- Same encoded artifact, distinct mHC/attention numerical class and binding versus strict controls.
- No independent full-model quality, high/maximum reasoning, HTTP or installed-product performance qualification.
- Advisory locks plus sampled clear resource observations do not prove continuous exclusivity.
- Native first-fragment arrival is not terminal painting.
- 4K coding/16K prefill admission does not qualify 32K candidate execution.
