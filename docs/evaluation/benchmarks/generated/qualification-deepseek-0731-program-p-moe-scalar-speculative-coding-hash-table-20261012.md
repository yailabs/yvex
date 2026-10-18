<!-- docs:metadata
title: "0731 explicit BF16/F32 matrices and scalar-worklist scheduling: moe-scalar / speculative / coding.hash-table"
id: yvex.evaluation.qualification.deepseek-0731-program-p-moe-scalar-speculative-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-moe-scalar-speculative-coding-hash-table-20261012.json
-->

# 0731 explicit BF16/F32 matrices and scalar-worklist scheduling: moe-scalar / speculative / coding.hash-table

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-moe-scalar-speculative-coding-hash-table-20261012.json)

Target identity: `efa3e2c608e2d5068c7aeaae074d74275bfa84859096a6a5b9d9047e971e65fe`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.hash-table/turn-0 | 0.654292 | s | 3 | 0.643147–0.743024 | 0.0111454 |
| request.client-complete / coding.hash-table/turn-0 | 18.9363 | s | 3 | 18.8845–19.0339 | 0.0517644 |
| ttft.client-visible / coding.hash-table/turn-0 | 1.87916 | s | 3 | 1.83357–1.9717 | 0.0455917 |
| delivery.client-gap.maximum / coding.hash-table/turn-0 | 0.436884 | s | 3 | 0.429431–0.464823 | 0.00745354 |
| delivery.client-gap.mean / coding.hash-table/turn-0 | 0.0669084 | s | 3 | 0.0666855–0.0670673 | 0.000158882 |
| ttft.server / coding.hash-table/turn-0 | 1.22486 | s | 3 | 1.19041–1.22866 | 0.00380069 |
| final.first.server / coding.hash-table/turn-0 | 1.22486 | s | 3 | 1.19041–1.22866 | 0.00380069 |
| final.first.client / coding.hash-table/turn-0 | 1.87916 | s | 3 | 1.83357–1.9717 | 0.0455917 |
| final.phase-rate / coding.hash-table/turn-0 | 14.623 | token/s | 3 | 14.5982–14.6685 | 0.0247624 |
| decode.post-first.committed / coding.hash-table/turn-0 | 14.9459 | token/s | 3 | 14.9105–14.9959 | 0.0354096 |
| prefill.uncached / coding.hash-table/turn-0 | 39.8986 | token/s | 3 | 39.5723–41.0086 | 0.326292 |
| prefill.wall / coding.hash-table/turn-0 | 0.77697 | s | 3 | 0.755939–0.783376 | 0.00640648 |

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
| Draft phase | 1.22236 | s | 3 | 1.2171–1.22748 | 0.00512213 |
| Verification phase | 10.9136 | s | 3 | 10.8525–10.9156 | 0.00202595 |
| Speculative commit phase | 5.04723 | s | 3 | 5.0465–5.05879 | 0.000726453 |

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

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.maximum**: per-turn maximum interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **delivery.client-gap.mean**: per-turn arithmetic mean interval between consecutive nonempty native final/reasoning fragments at client receive; excludes TTFT, includes observer work, not transport-only or terminal paint. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: native-v25 isolated resident host; selected configuration, not installed defaults. Session: fresh; warm/cold: resident engine after separate warmup; fresh session; file cache uncontrolled; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/moe-scalar-native-01/speculative/4096-coding.hash-table/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh-session samples, not a confidence interval.
- Same encoded artifact. BF16 matrix differs in numerical class/binding from mHC-stream; scalar scheduling preserves the BF16-matrix binding and arithmetic.
- No independent full-model quality, high/maximum reasoning, HTTP or installed-product performance qualification.
- Advisory locks plus sampled clear resource observations do not prove continuous exclusivity.
- Native first-fragment arrival is not terminal painting.
- 4K coding/16K prefill admission does not qualify 32K candidate execution.
