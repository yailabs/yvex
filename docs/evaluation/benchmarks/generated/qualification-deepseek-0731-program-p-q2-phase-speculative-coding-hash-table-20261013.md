<!-- docs:metadata
title: "0731 phase and binding ablation: q2-phase / speculative / coding.hash-table"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-phase-speculative-coding-hash-table-20261013
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-phase-speculative-coding-hash-table-20261013.json
-->

# 0731 phase and binding ablation: q2-phase / speculative / coding.hash-table

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-phase-speculative-coding-hash-table-20261013.json)

Target identity: `5008b1fe97663e97de9c019b311cf6287c7c4be0ed81549df7b49cce140b7c85`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.555718 | s | 3 | 0.546767–0.649413 | 0.00895058 |
| request.client-complete / coding.hash-table/turn-0 | 18.3941 | s | 3 | 18.3619–18.4979 | 0.0322125 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.59167 | s | 3 | 1.55825–1.7253 | 0.033417 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.443145 | s | 3 | 0.430903–0.450763 | 0.00761789 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0657729 | s | 3 | 0.0657635–0.0660207 | 9.40033e-06 |
| ttft.server / coding.hash-table/turn-0 | 1.04489 | s | 3 | 1.00251–1.07588 | 0.0309882 |
| final.first.server / coding.hash-table/turn-0 | 1.04489 | s | 3 | 1.00251–1.07588 | 0.0309882 |
| final.first.client / coding.hash-table/turn-0 | 1.59167 | s | 3 | 1.55825–1.7253 | 0.033417 |
| final.phase-rate / coding.hash-table/turn-0 | 14.8544 | token/s | 3 | 14.8043–14.8876 | 0.0331201 |
| decode.post-first.committed / coding.hash-table/turn-0 | 15.204 | token/s | 3 | 15.1468–15.2061 | 0.00215136 |
| prefill.uncached / coding.hash-table/turn-0 | 50.5067 | token/s | 3 | 50.1005–56.837 | 0.406255 |
| prefill.wall / coding.hash-table/turn-0 | 0.613779 | s | 3 | 0.545419–0.618756 | 0.00497701 |

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
| Draft cycles | 43 | cycle | 3 | 43–43 | 0 |
| Draft forwards | 43 | forward | 3 | 43–43 | 0 |
| Proposed | 215 | token | 3 | 215–215 | 0 |
| Selected verification | 213 | token | 3 | 213–213 | 0 |
| Target verifications | 43 | verification | 3 | 43–43 | 0 |
| Accepted draft | 170 | token | 3 | 170–170 | 0 |
| Rejected draft | 43 | token | 3 | 43–43 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | 43 | token | 3 | 43–43 | 0 |
| Per-sample mean accepted prefix | 3.95349 | token | 3 | 3.95349–3.95349 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 1.21906 | s | 3 | 1.21044–1.22384 | 0.00478065 |
| Verification phase | 10.5681 | s | 3 | 10.5582–10.653 | 0.00989541 |
| Speculative commit phase | 5.08931 | s | 3 | 5.08823–5.10857 | 0.00108174 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | Three uninstrumented fresh native samples; no independent checkpoint quality or installed rollout;  |
| checkpoint-reference | BLOCKED | No independent complete-checkpoint quality result; Independent checkpoint-matched held-out evidence missing |
| deployment-performance | CHARACTERIZED | Three uninstrumented fresh native samples; no independent checkpoint quality or installed rollout;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three uninstrumented fresh native samples; no independent checkpoint quality or installed rollout;  |
| representation-quality | BLOCKED | No independent complete-checkpoint quality result; Independent checkpoint-matched held-out evidence missing |

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
| binding | 0884e68a1ccb6362323bd27241c7b1bd2ed96d0fd712684a1dea7b6c2ce30c37 |
| specialization | 801bd3bcbc667379af8d4d9d4ec7fa6d927c909ecd6c3a5855fd77282d5a3a23 |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | ac932ecd8d958b5a38bf8a521f0c672d4694a4085ad119f8eff4edbe79cc8689 |
| build | 3320efd425c14059a6a20f2cc2008ac5a96f18d8af54830d994d0a1ecef8e570 |
| executable | 3a11abad19923147c18e3895105daa6ab0662f8bc307776af202242d2e383861 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; q2-phase admitted realization |
| kernel_bundle | a22675f812e5b96f44920f72a1e822e7d6c01233bd1bf54c216c364bc14c7f48 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-native-01/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh samples, not a confidence interval.
- Bounded component numerical controls do not establish independent whole-checkpoint quality.
- No high/maximum reasoning, HTTP or installed-product performance qualification.
- Advisory locks and sampled clear resources do not prove continuous exclusivity.
- Native fragment receive times are not terminal painting.
- 4K coding and 16K prefill do not qualify the candidate at 32K.
