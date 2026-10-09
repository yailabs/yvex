<!-- docs:metadata
title: "DeepSeek 0731 Q8/Q2: native load blocked by memory admission"
id: yvex.evaluation.qualification.deepseek-0731-native-load-refusal-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-native-load-refusal-20261009.json
-->

# DeepSeek 0731 Q8/Q2: native load blocked by memory admission

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-native-load-refusal-20261009.json)

Target identity: `cf323fcb612cf1e88ba9f3ca25af7291be31ed2e18e6b61297700b91a6ada6da`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| artifact.payload-bytes / native-load-admission | 1.07066195e+11 | byte | Encoded payload, included once in preflight; not measured residency; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-memory-preflight.json |
| admission.maximum-tensor-transient-bytes / native-load-admission | 1.05906176e+09 | byte | Existing preflight maximum encoded tensor transient; not an observed allocation; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-memory-preflight.json |
| admission.system-reserve-bytes / native-load-admission | 1.63328957e+10 | byte | Existing mandatory total-memory/8 reserve; unchanged; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-memory-preflight.json |
| admission.minimum-preflight-bytes / native-load-admission | 1.24458152e+11 | byte | Sum of payload, reserve and maximum tensor; excludes later startup-workspace qualification; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-memory-preflight.json |
| admission.available-at-accounting-probe / native-load-admission | 1.2040833e+11 | byte | Later read-only system capacity observation with old engine restored; not the exact failed-load instant; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/0731-memory-preflight.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | CHARACTERIZED | Four official encoding vectors plus artifact-bound BPE and bounded none/high/maximum prompt controls; not full tool/history semantics;  |
| checkpoint-reference | BLOCKED | Exact 0731 source; independent full-model checkpoint reference remains missing; No checkpoint-matched independent full-model output/logit evidence |
| representation-quality | BLOCKED | Native Q8_0/Q2_K artifact integrity is not quantization quality; No independent checkpoint-matched quality comparison for this physical variant |
| backend-execution | BLOCKED | 0731 full-model execution and timing unearned; Native engine load refuses before residency: minimum preflight is 124458152440 bytes; current host-preserving system window has about 120.4 GB available. No 0731 forward executed. A service-memory/reconfiguration window or separately admitted smaller representation is required; this is not a hardware throughput ceiling. |
| deployment-performance | BLOCKED | 0731 full-model execution and timing unearned; Native engine load refuses before residency: minimum preflight is 124458152440 bytes; current host-preserving system window has about 120.4 GB available. No 0731 forward executed. A service-memory/reconfiguration window or separately admitted smaller representation is required; this is not a hardware throughput ceiling. |
| product-path | BLOCKED | Observed fail-closed native load; no generation or rollout; Native engine load refuses before residency: minimum preflight is 124458152440 bytes; current host-preserving system window has about 120.4 GB available. No 0731 forward executed. A service-memory/reconfiguration window or separately admitted smaller representation is required; this is not a hardware throughput ceiling. |

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
| source_commit | 4ecc2b6b4abde7e76e34ba3758b946842f0a04f3 |
| source_tree | 599754283780af35ae3b6bfcda2f5f1448741207 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | 90f1b0db1d8341e7608359d5dc68fa665637f26cd86c773d6922a82311a515ef |
| executable | f3d1611be8a322d080117380d9cf61591f0544173cc20071095d85e5a9c53c72 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | requested 4096 context; native host isolated; no engine admitted |
| context | 4096 |
| prefill_geometry | not admitted; configured CUDA default 512 not reached |
| sequence_geometry | requested width 1; no session created |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none intended; no request submitted |
| sampling | greedy intended; no request submitted |
| product_path | isolated Rust host / native protocol v25 load admission |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Failed load is not a zero-throughput measurement. No new prefill, decode, TTFT or quality result exists.
- Per-plane family/reference/quality history is inherited only for this exact artifact/checkpoint; it does not qualify this new build.
- The reserve is unchanged. No operator model was loaded concurrently with the candidate; finite generation 2 and installed listeners remained available.
- Operator authorized closure of main and temporary old DeepSeek unloading. The same old artifact/binding/profile was restored at generation 19; session context was not replayed.
- Unaccepted CUDA candidate remains outside the clean measured build. Page warming and managed prefetch negatives were not rerun.
- Restarting the installed host is not authorized implicitly; a separate coordinated request was issued. No physical impossibility or performance ceiling is established.
