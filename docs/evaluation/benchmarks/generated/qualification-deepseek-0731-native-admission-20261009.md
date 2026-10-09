<!-- docs:metadata
title: "DeepSeek 0731 native artifact and binding; inference unqualified"
id: yvex.evaluation.qualification.deepseek-0731-native-admission-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-native-admission-20261009.json
-->

# DeepSeek 0731 native artifact and binding; inference unqualified

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-native-admission-20261009.json)

Target identity: `664f411c2e0c8f49789a78f24ab2495f3771fa89b5aa9c2726a00038a7341548`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| artifact.file-bytes / preparation | 1.07077901e+11 | byte | Complete emitted file extent; not resident memory; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-emission.log |
| artifact.payload-bytes / preparation | 1.07066195e+11 | byte | Encoded tensor bytes included in file extent; not additive memory; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-official-reader.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | CHARACTERIZED | Four official encoding vectors plus artifact-bound BPE and bounded none/high/maximum prompt controls; not full tool/history semantics;  |
| checkpoint-reference | BLOCKED | Exact 0731 source; independent full-model checkpoint reference remains missing; No checkpoint-matched independent full-model output/logit evidence |
| representation-quality | BLOCKED | Native Q8_0/Q2_K artifact integrity is not quantization quality; No independent checkpoint-matched quality comparison for this physical variant |
| backend-execution | UNQUALIFIED | No 0731 model forward executed;  |
| deployment-performance | UNQUALIFIED | No 0731 prefill/decode/load benchmark; 20/700 remain unearned;  |
| product-path | UNQUALIFIED | No 0731 hosted generation or installed-product rollout;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | d66294b5e016806f2e283e8b0160e25385302fa6c9db4ac24a49e94140fa0ea9 |
| representation | deepseek-v4-flash-0731-q8_0-q2_k-v1 |
| artifact_set | 4dc4265a92d77c874c82aa16c1358688b71bb7b10267cc80911c3ef42d2fa11a |
| binding | 29676295361a8c99b86b3e1837a64e23791337f862d01fbeaeb2b5361399cc6d |
| specialization | NOT RETAINED |
| source_commit | 7ed9e4b7b9fba21d00d4f493ba9d34e0be877044 |
| source_tree | 66f9de8b8997a4869bdcc3c805098f8935325711 |
| source_delta | 7f84809883c7564577a7701f1708d274461b00127aca1c8fda79bf621d3f5d03 |
| build | b6411f322ec55b4da4876b1b5efbb0d55e059eff4ab2715a9d0f8b46de51507f |
| executable | 1e08904c132fdba8808572430945e7bed53c3d0d018c49b99b9f90fea8632fc9 |
| backend | NOT RETAINED |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | NOT RETAINED |
| context | NOT RETAINED |
| prefill_geometry | NOT RETAINED |
| sequence_geometry | NOT RETAINED |
| concurrency | NOT RETAINED |
| strategy | NOT RETAINED |
| reasoning | NOT RETAINED |
| sampling | NOT RETAINED |
| product_path | native offline preparation and artifact-bound tokenizer; no inference |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Preparation and tokenizer admission do not qualify forward execution, numerical behavior, quality, performance or release readiness.
- 0731 Q8_0/Q2_K differs from DwarfStar IQ2_XXS and the prior YVEX mixed variant; do not attribute changed quantization to engine speed.
- 43 preserved F32 controls match the external 0731 artifact; complete target/support checkpoint equivalence remains UNPROVEN.
- No GPU engine was loaded for this record. Deployment identities and geometry remain null, not inferred from the operator's old engine.
- Installed DeepSeek generation 18 and finite generation 2 remain unchanged.
