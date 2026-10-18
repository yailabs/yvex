<!-- docs:metadata
title: "0731 explicit BF16/F32 matrices and scalar-worklist scheduling: moe-scalar / target-only / prefill.promessi-2048"
id: yvex.evaluation.qualification.deepseek-0731-program-p-moe-scalar-target-only-prefill-promessi-2048-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-moe-scalar-target-only-prefill-promessi-2048-20261012.json
-->

# 0731 explicit BF16/F32 matrices and scalar-worklist scheduling: moe-scalar / target-only / prefill.promessi-2048

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-moe-scalar-target-only-prefill-promessi-2048-20261012.json)

Target identity: `85b0944cd55639e6b5a6746e9278ebb70872a3d3e686945f7ccb8b1b85a11a20`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / prefill.promessi-2048/turn-0 | 0.670261 | s | 3 | 0.662835–0.688349 | 0.00742529 |
| request.client-complete / prefill.promessi-2048/turn-0 | 15.0714 | s | 3 | 15.0195–15.079 | 0.00752727 |
| ttft.client-visible / prefill.promessi-2048/turn-0 | 13.453 | s | 3 | 13.3949–13.4592 | 0.00619071 |
| delivery.client-gap.maximum / prefill.promessi-2048/turn-0 | 0.116218 | s | 3 | 0.112626–0.116844 | 0.000626239 |
| delivery.client-gap.mean / prefill.promessi-2048/turn-0 | 0.108301 | s | 3 | 0.107477–0.108392 | 9.11161e-05 |
| ttft.server / prefill.promessi-2048/turn-0 | 12.7646 | s | 3 | 12.732–12.7888 | 0.0242139 |
| final.first.server / prefill.promessi-2048/turn-0 | 12.7646 | s | 3 | 12.732–12.7888 | 0.0242139 |
| final.first.client / prefill.promessi-2048/turn-0 | 13.453 | s | 3 | 13.3949–13.4592 | 0.00619071 |
| final.phase-rate / prefill.promessi-2048/turn-0 | 9.2129 | token/s | 3 | 9.20333–9.23368 | 0.0095763 |
| prefill.uncached / prefill.promessi-2048/turn-0 | 161.882 | token/s | 3 | 161.676–162.295 | 0.206364 |
| prefill.wall / prefill.promessi-2048/turn-0 | 12.6512 | s | 3 | 12.619–12.6673 | 0.0161481 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### prefill.promessi-2048/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 2048 | token | 3 | 2048–2048 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 2048 | token | 3 | 2048–2048 | 0 |
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
| backend-execution | CHARACTERIZED | Three isolated native requests; explicit numerical class with equivalent MoE scheduling where selected; no installed rollout or independent checkpoint quality;  |
| checkpoint-reference | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| deployment-performance | CHARACTERIZED | Three isolated native requests; explicit numerical class with equivalent MoE scheduling where selected; no installed rollout or independent checkpoint quality;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three isolated native requests; explicit numerical class with equivalent MoE scheduling where selected; no installed rollout or independent checkpoint quality;  |
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
| binding | f01c0d0ce14d7c48d0adb582f903d48fbe1f322c59799292aadad2820c8abee3 |
| specialization | 92dec75ee03479cba2905305a424f0dd9bba421b1fa5695bf46f58033e898d85 |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | e9987a3c22b88ebe802d0c0d40d7d0698a8ad06ded4b62197a453344a21faf01 |
| build | ac1399cc141587d33865be141c580d4bd7f0be89ba7886964115ee1ce32100db |
| executable | cb9cf0a9a8ba3c197cb3923213da2423ea90b4ef52cd679a605668ac3a844ae2 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; admitted mHC F32 and BF16-stored matrix; moe-scalar executable realization |
| kernel_bundle | e0d604aa4d24abab240b7fdd34d8dcf1f5d14dfeea03d52b416c0cdd9bdd1100 |
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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 16. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/target-only/16384-prefill.promessi-2048/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples, not a confidence interval.
- Same encoded artifact. BF16 matrix differs in numerical class/binding from mHC-stream; scalar scheduling preserves the BF16-matrix binding and arithmetic.
- No independent full-model quality, high/maximum reasoning, HTTP or installed-product performance qualification.
- Advisory locks plus sampled clear resource observations do not prove continuous exclusivity.
- Native first-fragment arrival is not terminal painting.
- 4K coding/16K prefill admission does not qualify 32K candidate execution.
