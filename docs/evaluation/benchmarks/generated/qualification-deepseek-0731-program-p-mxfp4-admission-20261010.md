<!-- docs:metadata
title: "Program P 0731 MXFP4 with routed Q2: authenticated artifact and native load"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mxfp4-admission-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mxfp4-admission-20261010.json
-->

# Program P 0731 MXFP4 with routed Q2: authenticated artifact and native load

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mxfp4-admission-20261010.json)

Target identity: `e92461c22e97e8cbfedf02a8bf85b5628ede1eafe910023932490028d3b42255`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| deployment-performance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| product-path | CHARACTERIZED | Authenticated exact artifact/binding loads on isolated native host; benchmark refused before prompt because client catalog lacked this relationship;  |

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
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 516b569f2204993ac54a4a1a61535b8cf26781ede78fe8d9a7b8995abd4d1072 |
| build | 407b1df74781f1d78b0459f94f874475ce2ae5667552e668646524466ff16a1e |
| executable | 4507f4f3ed6a4e10e92c04b6b2975c189ea48f4630d51f33b789f15aaa98862a |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated native host; context 4096; loaded, no prompt executed |
| context | 4096 |
| prefill_geometry | configured chunk 512; no input positions executed |
| sequence_geometry | width 1; no session created |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature 1; no stochastic draws |
| product_path | native protocol v25 isolated host; load only |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No inference, throughput or independent representation-quality claim.
- Pinned GGUF reader establishes structure, not numerical quality.
- Load included file verification and uncontrolled file-cache state; no comparative load-speed claim.
- Installed Host was untouched; isolated host cleanly stopped after pre-request qualification refusal.
- Same checkpoint lineage does not imply same quantized weights as Q2 or DwarfStar.
