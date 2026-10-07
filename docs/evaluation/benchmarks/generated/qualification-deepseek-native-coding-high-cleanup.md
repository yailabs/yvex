<!-- docs:metadata
title: "DeepSeek native coding/high refusal and owned-session cleanup"
id: yvex.evaluation.qualification.deepseek-native-coding-high-cleanup
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-coding-high-cleanup.json
-->

# DeepSeek native coding/high refusal and owned-session cleanup

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-coding-high-cleanup.json)

Target identity: `c5fddba68ea6136653abba6368504645c706b6dec42470c94cc938a1a512a3ae`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| coding.metal | FAIL | YVEX_ERR_FORMAT at256 without reasoning terminator; runner correctly refused measurement and closed its owned session |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | UNQUALIFIED | No completed successful sample; a failed bounded request is not a throughput sample;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | One native high-mode coding request fails closed at the declared 256-token bound before the source reasoning terminator;  |
| representation-quality | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |

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
| build | ef64f4cf0defe06720c6cf815640ba5c9522ca20838be535d3f5bdfcd4990e4e |
| executable | 037ba2bc36d68893a6389102294f3efb17923adc357138525d4f1f79faea9a76 |
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

- Request outcome FAIL remains visible; correctly handled refusal and cleanup do not turn it into successful inference or a performance sample.
- One fresh isolated synthetic case, no retry of operator/Case content. Output bound and reasoning policy unchanged.
- Typed correlated terminal refusal, exact owned session retired, zero active/queued work and sessions before supported isolated-host stop; no successful performance receipt emitted.
- Not cancellation coverage, numerical quality, source-parser conformance, long-context evidence or the throughput Task exit.
