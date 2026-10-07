<!-- docs:metadata
title: "DeepSeek native coding high \u2014 bounded reasoning refusal"
id: yvex.evaluation.qualification.deepseek-native-coding-high-bounded-refusal
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-coding-high-bounded-refusal.json
-->

# DeepSeek native coding high — bounded reasoning refusal

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-coding-high-bounded-refusal.json)

Target identity: `5924b0b658d0625ec7238b3470b148668aee00aebedd42057dbedcc45b628718`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| coding.metal | FAIL | YVEX_ERR_FORMAT: thinking ended before its source delimiter at the 256-token output bound; request failed, not numerical backend status |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | UNQUALIFIED | No completed successful sample; a failed bounded request is not a throughput sample;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | One native high-mode coding request fails closed at the declared 256-token bound before the source reasoning terminator;  |
| representation-quality | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | NOT RETAINED |
| build | 4b6e3d63aeea948c8e894b864f5f59d0ff9d9d25d6393b7d18dd76d11abb686d |
| executable | dea948848e991351acbbbcf80afcd977f460f71e8a0ab3ddcfee94cc36705863 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | NOT RETAINED |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | high |
| sampling | explicit greedy; temperature=0 top_p=1; no penalties; no seed; native stochastic=0, HTTP temperature=0 |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One failed bounded request, not a median or a performance improvement. No successful performance receipt was emitted.
- The declared corpus bound was not raised and the reasoning policy was not changed to manufacture completion.
- Reasoning-to-final transition NOT MEASURED. A missing terminator at output capacity does not by itself demonstrate a numerical defect.
- The old runner conservatively labeled this terminal refusal unsettled. Typed session reconciliation established partial state before scoped cleanup; runner handling is being qualified separately.
- This observation does not qualify maximum mode, target-only, other prompts or 2K/8K prefill.
- No independently admitted logit tolerance or original high-precision quality comparison. Source snapshot is retained, but the aborted driver did not produce its final source-stability receipt.
