<!-- docs:metadata
title: "0731 projection realization CUPTI attribution: coding.hash-table (not timing evidence)"
id: yvex.evaluation.qualification.deepseek-0731-program-p-projection-critical-path-coding-hash-table-20261012
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-projection-critical-path-coding-hash-table-20261012.json
-->

# 0731 projection realization CUPTI attribution: coding.hash-table (not timing evidence)

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-projection-critical-path-coding-hash-table-20261012.json)

Target identity: `11942ee9c3da89847bc7bd8c0d392790f30aa49514826092c3c6b08dde724350`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| coding.hash-table.kernel-sum / coding.hash-table | 6.08143684 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_qtype_matvec / coding.hash-table | 1.38187039 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_moe_grouped_up_q2_rows / coding.hash-table | 0.931900468 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_decoded_mxfp4_rows / coding.hash-table | 0.486901347 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_moe_grouped_down_q2_rows / coding.hash-table | 0.478478388 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_mxfp4_q8_rows / coding.hash-table | 0.44008264 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_residual_mhc_pre / coding.hash-table | 0.375955164 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_mxfp4_q8_matvec_8192 / coding.hash-table | 0.339045936 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_mxfp4_q8_matvec_4096 / coding.hash-table | 0.310950853 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_yarn_rope / coding.hash-table | 0.194237431 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_reduce_native / coding.hash-table | 0.18713907 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_activation_quantize / coding.hash-table | 0.136934692 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |
| coding.hash-table.yvex_attention_weighted_norm / coding.hash-table | 0.131943094 | s | Complete-request CUPTI duration population including bounded decode; not an additive wall-time partition, throughput or measured memory bandwidth; /home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/projection-geometry-profile-01/coding.hash-table/cupti.tsv |

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
| source_delta | e6bd839816b21992c16d50a02df16b88ac9688ff4548c05da35a80e8a011a9e3 |
| build | 3b88b6ad351e3a19d406f0bdefb97f64ae8da0f902185421936d5b6316d90aa1 |
| executable | bdf4848cb9617229a33989815da734f5f72ff3cc0b1e636daeaa5ba13660edbb |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; MXFP4 nonrouted; projection executable realization |
| kernel_bundle | e8065adfe0ea291901c18eb817a12d142a026a973aa28dc324314d56297b2acf |
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
