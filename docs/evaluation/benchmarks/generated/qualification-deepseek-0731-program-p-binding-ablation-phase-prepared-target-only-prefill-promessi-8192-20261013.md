<!-- docs:metadata
title: "0731 phase and binding ablation: binding-ablation-phase-prepared / target-only / prefill.promessi-8192"
id: yvex.evaluation.qualification.deepseek-0731-program-p-binding-ablation-phase-prepared-target-only-prefill-promessi-8192-20261013
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-binding-ablation-phase-prepared-target-only-prefill-promessi-8192-20261013.json
-->

# 0731 phase and binding ablation: binding-ablation-phase-prepared / target-only / prefill.promessi-8192

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-binding-ablation-phase-prepared-target-only-prefill-promessi-8192-20261013.json)

Target identity: `86ef8f98480cd84146c6edc5c69ed56f611fb8a184de24191dc246da8cdd85d5`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-8192/turn-0 | 0.523085 | s | 3 | 0.516898–0.54157 | 0.00618622 |
| request.client-complete / prefill.promessi-8192/turn-0 | 38.9065 | s | 3 | 38.833–38.9408 | 0.0343267 |
| ttft.client-visible / prefill.promessi-8192/turn-0 | 37.3057 | s | 3 | 37.2168–37.3306 | 0.0248505 |
| delivery.client-gap.maximum / prefill.promessi-8192/turn-0 | 0.112735 | s | 3 | 0.111722–0.114075 | 0.0010135 |
| delivery.client-gap.mean / prefill.promessi-8192/turn-0 | 0.10734 | s | 3 | 0.106708–0.107739 | 0.000399054 |
| ttft.server / prefill.promessi-8192/turn-0 | 36.7888 | s | 3 | 36.6752–36.8074 | 0.0186606 |
| final.first.server / prefill.promessi-8192/turn-0 | 36.7888 | s | 3 | 36.6752–36.8074 | 0.0186606 |
| final.first.client / prefill.promessi-8192/turn-0 | 37.3057 | s | 3 | 37.2168–37.3306 | 0.0248505 |
| final.phase-rate / prefill.promessi-8192/turn-0 | 9.29283 | token/s | 3 | 9.26759–9.3513 | 0.0252315 |
| prefill.uncached / prefill.promessi-8192/turn-0 | 223.361 | token/s | 3 | 223.255–224.055 | 0.106161 |
| prefill.wall / prefill.promessi-8192/turn-0 | 36.676 | s | 3 | 36.5624–36.6934 | 0.01744 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-8192/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 8192 | token | 3 | 8192–8192 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 8192 | token | 3 | 8192–8192 | 0 |
| Committed output | 16 | token | 3 | 16–16 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 16 | token | 3 | 16–16 | 0 |
| Draft cycles | 0 | cycle | 3 | 0–0 | 0 |
| Draft forwards | 0 | forward | 3 | 0–0 | 0 |
| Proposed | 0 | token | 3 | 0–0 | 0 |
| Selected verification | 0 | token | 3 | 0–0 | 0 |
| Target verifications | 0 | verification | 3 | 0–0 | 0 |
| Accepted draft | 0 | token | 3 | 0–0 | 0 |
| Rejected draft | 0 | token | 3 | 0–0 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 0 | token | 3 | 0–0 | 0 |
| Per-sample mean accepted prefix | 0 | token | 3 | 0–0 | 0 |
| Per-sample maximum accepted prefix | 0 | token | 3 | 0–0 | 0 |
| Draft phase | 0 | s | 3 | 0–0 | 0 |
| Verification phase | 0 | s | 3 | 0–0 | 0 |
| Speculative commit phase | 0 | s | 3 | 0–0 | 0 |

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
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; binding-ablation-phase-prepared admitted realization |
| kernel_bundle | a22675f812e5b96f44920f72a1e822e7d6c01233bd1bf54c216c364bc14c7f48 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/binding-ablation-native-01/phase-prepared/16384-prefill.promessi-8192/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh samples, not a confidence interval.
- Bounded component numerical controls do not establish independent whole-checkpoint quality.
- No high/maximum reasoning, HTTP or installed-product performance qualification.
- Advisory locks and sampled clear resources do not prove continuous exclusivity.
- Native fragment receive times are not terminal painting.
- 4K coding and 16K prefill do not qualify the candidate at 32K.
- Same frozen executable; bindings select explicitly different numerical classes. Not an equivalent-arithmetic comparison.
