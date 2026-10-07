<!-- docs:metadata
title: "DeepSeek installed native coding.metal / maximum \u2014 bounded reasoning refusal"
id: yvex.evaluation.qualification.deepseek-installed-metal-maximum-42
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-metal-maximum-42.json
-->

# DeepSeek installed native coding.metal / maximum — bounded reasoning refusal

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-metal-maximum-42.json)

Target identity: `f6188c3afebb8489e224e0401fc1c3728f00522972734659e486bb56fafecca3`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| committed-before-refusal / coding.metal | 256 | count | Partial committed population before reset-required refusal; not successful completion or a throughput denominator; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-product-remaining-v42/coding.metal-maximum/native/events.jsonl |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| coding.metal | FAIL | Bounded turn refused before source reasoning delimiter; successful performance not measured |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |
| checkpoint-reference | UNQUALIFIED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |
| deployment-performance | UNQUALIFIED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |
| family-conformance | UNQUALIFIED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |
| product-path | CHARACTERIZED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |
| representation-quality | UNQUALIFIED | One bounded source-grammar refusal and session cleanup; no successful reasoning-performance claim;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | 4d489a7e6340ca30f5cd6e72af004347eb246e2b9521eaf6f7c6ae56b5f995dd |
| transformation_ir | f1fca7b4ec04d1b0de2a0f0707b3f78c5600e9a6486a83c6fc9f3a4bd70f88e8 |
| physical_policy | 59dd7bdabf6b81989dfa14e0f70692805a8f02a473afcc040a3e55083f48dda0 |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | a444bcdd384f6abfc79b07d1a26d93c17c4a97c0 |
| source_tree | 508e7cfeb5887fc123e200038d073ce72dc79049 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | f748ad6114858e5f1b6246f8bd2d6ade9a1743bd72e8e27b351d951c35b00bac |
| executable | e1ea8e02ad223a3fffb2ecc6839ee354658bd97a6e23c7e6b0651007d4ad3b4d |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 6438a5e3b5e9b01b2f43c05922eb6b05df9bfdae68d8aa74e3dbbb1c06118ba1 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | maximum |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- LOCAL evidence, not YVEX-published model-quality, release or 20/700 target qualification.
- Exact clean producer source/build is bound externally; typed hardware/kernel facts unavailable, never inferred.
- Sampled process lists are not uninterrupted hardware exclusivity.
- No profiler or periodic smaps walk inside timed requests; external instrumentation declaration has bounded scope.
- Authored representative prompts are not official DeepSeek upstream encoding or inference vectors.
- Complete coding output is bounded at 256 tokens; no long-context or unlimited-completion claim.
- First fragment publication remains unavailable; internal and client-visible TTFT remain separate.
- Reasoning delimiter was not reached at the bound; final transition and successful decode are NOT MEASURED.
