<!-- docs:metadata
title: "0731 online64 realization CUPTI attribution: coding.hash-table (not timing evidence)"
id: yvex.evaluation.qualification.deepseek-0731-program-p-online64-critical-path-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-online64-critical-path-coding-hash-table-20261012.json
-->

# 0731 online64 realization CUPTI attribution: coding.hash-table (not timing evidence)

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-online64-critical-path-coding-hash-table-20261012.json)

Target identity: `1df98afc5d525782716ab3839c339cecd0c72d41372bb88056ca2dcf1d455436`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| coding.hash-table.kernel-sum / coding.hash-table | 6.21601319 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_qtype_matvec / coding.hash-table | 2.22807944 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_moe_grouped_up_q2_rows / coding.hash-table | 0.777138642 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_decoded_mxfp4_rows / coding.hash-table | 0.486599388 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_mxfp4_q8_rows / coding.hash-table | 0.439455283 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_moe_grouped_down_q2_rows / coding.hash-table | 0.420957924 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_block_qk / coding.hash-table | 0.399177156 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_residual_mhc_pre / coding.hash-table | 0.376421099 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_yarn_rope / coding.hash-table | 0.195791274 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_activation_quantize / coding.hash-table | 0.138002784 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_weighted_norm / coding.hash-table | 0.132473298 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_qtype_tensorcore_rows / coding.hash-table | 0.094447419 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_block_pv / coding.hash-table | 0.089007746 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/online64-profile-01/coding.hash-table/cupti.tsv |

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
| binding | 44c8bf1dc9cd3275f840aaf06efbab70dadf0f72a24af92b17aa60c14b31c236 |
| specialization | NOT RETAINED |
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
| runtime_configuration | controlled fresh engine; bounded diagnostic output; instrumented |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine CUPTI diagnostic |
| suite | d5d9eb417abe63fc2d2c242317479cfc785ce46688e25cc7e936501111393f9d |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Profiled diagnostic with exact input/output population, not an uninstrumented performance sample.
- Overlapping host scopes and GPU intervals cannot be summed as disjoint request phases.
- Kernel duration/population does not establish physical bandwidth, occupancy or a hardware ceiling.
- No independent checkpoint quality or installed-runtime promotion.
