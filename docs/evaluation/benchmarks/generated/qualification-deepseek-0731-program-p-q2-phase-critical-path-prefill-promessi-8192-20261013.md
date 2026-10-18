<!-- docs:metadata
title: "0731 phase-specific Q2 CUPTI attribution: prefill.promessi-8192 (not timing evidence)"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-phase-critical-path-prefill-promessi-8192-20261013
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-phase-critical-path-prefill-promessi-8192-20261013.json
-->

# 0731 phase-specific Q2 CUPTI attribution: prefill.promessi-8192 (not timing evidence)

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-phase-critical-path-prefill-promessi-8192-20261013.json)

Target identity: `80e315465010edb1756f08a45f49b16f0ffab5b6b33909a95c92b34bbc490e69`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| prefill.promessi-8192.kernel_sum_seconds / prefill.promessi-8192 | 34.869572 | s | Instrumented complete-request interval/population; not a disjoint wall-time share or throughput; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-critical-gdb-01/prefill.promessi-8192/kernel-populations.json |
| prefill.promessi-8192.kernel_union_seconds / prefill.promessi-8192 | 34.8518509 | s | Instrumented complete-request interval/population; not a disjoint wall-time share or throughput; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-critical-gdb-01/prefill.promessi-8192/kernel-populations.json |
| prefill.promessi-8192.kernel_or_copy_union_seconds / prefill.promessi-8192 | 36.0674827 | s | Instrumented complete-request interval/population; not a disjoint wall-time share or throughput; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-critical-gdb-01/prefill.promessi-8192/kernel-populations.json |
| prefill.promessi-8192.span_seconds / prefill.promessi-8192 | 39.9921636 | s | Instrumented complete-request interval/population; not a disjoint wall-time share or throughput; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-critical-gdb-01/prefill.promessi-8192/kernel-populations.json |
| prefill.promessi-8192.unobserved_gap_seconds / prefill.promessi-8192 | 3.92468094 | s | Instrumented complete-request interval/population; not a disjoint wall-time share or throughput; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/q2-phase-critical-gdb-01/prefill.promessi-8192/kernel-populations.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | UNQUALIFIED | Instrumented attribution only; native measurements separately retained;  |
| deployment-performance | UNQUALIFIED | Instrumented attribution only; native measurements separately retained;  |
| product-path | UNQUALIFIED | Instrumented attribution only; native measurements separately retained;  |

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
| specialization | NOT RETAINED |
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
| runtime_configuration | controlled fresh engine; bounded diagnostic output; instrumented |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine CUPTI diagnostic with GDB wrapper |
| suite | c155726e2fc7bed83c828162fcd7a61ccb9223a56f4c137ad32cf3ef9dadeebe |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Profiled diagnostic with exact input/output population, not an uninstrumented performance sample.
- Overlapping host scopes and GPU intervals cannot be summed as disjoint request phases.
- Kernel duration/population does not establish physical bandwidth, occupancy or a hardware ceiling.
- No independent checkpoint quality or installed-runtime promotion.
- GDB wrapper, CUPTI and launch-argument interception are present; use separate native N=3 receipts for performance.
