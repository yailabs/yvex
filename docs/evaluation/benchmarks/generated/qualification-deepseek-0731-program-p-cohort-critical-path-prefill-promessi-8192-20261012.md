<!-- docs:metadata
title: "0731 cohort realization CUPTI attribution: prefill.promessi-8192 (not timing evidence)"
id: yvex.evaluation.qualification.deepseek-0731-program-p-cohort-critical-path-prefill-promessi-8192-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-cohort-critical-path-prefill-promessi-8192-20261012.json
-->

# 0731 cohort realization CUPTI attribution: prefill.promessi-8192 (not timing evidence)

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-cohort-critical-path-prefill-promessi-8192-20261012.json)

Target identity: `675f605a63d779770a1a00cce896b6ffd4389970e9ecc3b3c2a80a41bda426da`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| prefill.promessi-8192.kernel-sum / prefill.promessi-8192 | 70.0909213 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_attention_reduce_native_warp_512 / prefill.promessi-8192 | 10.7501986 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_moe_grouped_up_shared / prefill.promessi-8192 | 10.24468 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_mxfp4_tensorcore_shared_rows / prefill.promessi-8192 | 8.23945392 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_qtype_matvec / prefill.promessi-8192 | 7.85579323 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_decoded_rows / prefill.promessi-8192 | 5.9253051 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_moe_grouped_down_shared / prefill.promessi-8192 | 4.14744435 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_q8_row_matrix / prefill.promessi-8192 | 3.45238857 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_moe_grouped_up_rows / prefill.promessi-8192 | 3.12163786 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_attention_yarn_rope / prefill.promessi-8192 | 2.94202907 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_attention_candidate_scores_matrix / prefill.promessi-8192 | 2.44681144 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_moe_grouped_down_rows / prefill.promessi-8192 | 2.14050323 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |
| prefill.promessi-8192.yvex_attention_rolling_rows / prefill.promessi-8192 | 1.37833636 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/cohort-geometry-profile-01/prefill.promessi-8192/cupti.tsv |

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
| binding | 091f933e11d8dd129e86d186f622c20f637ffe98b23d2d78fe118bd1a2199632 |
| specialization | NOT RETAINED |
| source_commit | ceeca35f5fa5b12b243cf4e61752cabfd2f15afd |
| source_tree | 9299178b4acb9c5fdeb05610057f31618c349cc9 |
| source_delta | 77c25bba4225d2fb62d8c249a98bc2baafabc54180d3255446ea445a13f22efd |
| build | d7bdb6fbe6abb229c9e4e3a53e88224facead81e61afb3cf7f152ee0a0e4f1b9 |
| executable | 0fdacecf85222c9626928793d976314b6dc42faa19936872d92cd15163139ed7 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; cohort executable realization |
| kernel_bundle | a9991d87a2d83742c833e66c148e78e05ffc7f98dcd8eb3b45ea21803a5ba723 |
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
| product_path | controlled-engine CUPTI diagnostic |
| suite | ba06670744e623b715778ff9f208f54da79ac0d9899184436ca4b7b229c04c1e |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Profiled diagnostic with exact input/output population, not an uninstrumented performance sample.
- Overlapping host scopes and GPU intervals cannot be summed as disjoint request phases.
- Kernel duration/population does not establish physical bandwidth, occupancy or a hardware ceiling.
- No independent checkpoint quality or installed-runtime promotion.
