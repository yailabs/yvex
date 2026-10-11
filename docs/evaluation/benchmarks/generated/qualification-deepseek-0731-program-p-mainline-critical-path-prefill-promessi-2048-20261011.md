<!-- docs:metadata
title: "0731 MXFP4/Q2 mainline CUPTI attribution: prefill.promessi-2048 (profiled; not throughput evidence)"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mainline-critical-path-prefill-promessi-2048-20261011
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mainline-critical-path-prefill-promessi-2048-20261011.json
-->

# 0731 MXFP4/Q2 mainline CUPTI attribution: prefill.promessi-2048 (profiled; not throughput evidence)

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mainline-critical-path-prefill-promessi-2048-20261011.json)

Target identity: `3c8fefecdc0546bcaf6035257ddf12b8ff6acdccaf93e5c1cd6dff878ed1e386`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| prefill.promessi-2048.kernel-sum / prefill.promessi-2048 | 22.2857406 | s | Sum of CUPTI kernel durations over this complete profiled request, including prefill and bounded decode; not additive host wall time or sustained throughput; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_moe_grouped_up_tensorcore / prefill.promessi-2048 | 3.88847561 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_mxfp4_tensorcore_wide_rows / prefill.promessi-2048 | 3.49806611 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_qtype_matvec / prefill.promessi-2048 | 2.60190281 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_attention_reduce_native_warp / prefill.promessi-2048 | 2.32633599 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_decoded_rows / prefill.promessi-2048 | 1.71069452 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_moe_grouped_down_tensorcore / prefill.promessi-2048 | 1.60164515 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_moe_grouped_up_rows / prefill.promessi-2048 | 0.930598798 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |
| prefill.promessi-2048.yvex_q8_row_matrix / prefill.promessi-2048 | 0.891368405 | s | CUPTI kernel population duration, includes prefill and bounded decode; denominator is complete trace kernel sum, not host wall; /home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg/diagnostic/prefill.promessi-2048/cupti.tsv |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | UNQUALIFIED | Instrumented attribution only; native performance has separate receipts;  |
| deployment-performance | UNQUALIFIED | Instrumented attribution only; native performance has separate receipts;  |
| product-path | UNQUALIFIED | Instrumented attribution only; native performance has separate receipts;  |

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
| specialization | NOT RETAINED |
| source_commit | 3c73c2a5a295cb8c3022effc4395441a75cd8f3c |
| source_tree | 7fcc485fbaba0f5fb7d72e472d8e7ed2fc8a354d |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | 16a312112794861fc6e444b68e26f1a14b4fd88e189af533af9bcef4ef1b01fa |
| executable | 5ea780c3f3c6b07f1822944059a03ae56b3190ad80537f41b68340eed5dc6c8e |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted candidate |
| kernel_bundle | fe4eac599c25a7a6fbb33b904383569c73993118496fc264e6f24c6a2aaf13f2 |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | controlled fresh engine; no repeated steady samples; instrumented |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine CUPTI diagnostic |
| suite | ba06670744e623b715778ff9f208f54da79ac0d9899184436ca4b7b229c04c1e |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Profiled diagnostic only; exact argv, context and bounded output population are retained. No performance samples or model quality promotion.
- Host attention and MoE scopes overlap/asynchronously include waits; do not add them or call them disjoint GPU costs.
- Logical transfer/active-weight counters are not physical DRAM traffic. Kernel duration alone cannot establish bandwidth, occupancy or a hardware ceiling.
- No retained numerical/layout change or speedup follows from profiling. Independent quality and competitive goals remain open.
