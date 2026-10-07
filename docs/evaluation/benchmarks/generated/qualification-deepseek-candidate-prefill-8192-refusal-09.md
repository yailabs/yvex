<!-- docs:metadata
title: "DeepSeek candidate \u2014 8K repeat numerical refusal and scoped cleanup"
id: yvex.evaluation.qualification.deepseek-candidate-prefill-8192-refusal-09
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-candidate-prefill-8192-refusal-09.json
-->

# DeepSeek candidate — 8K repeat numerical refusal and scoped cleanup

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-candidate-prefill-8192-refusal-09.json)

Target identity: `cea3e3a9de79c875f41f9b4c0c3567519edaac7515a763dbc42d20ff1e63728b`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-client / engine-load | 8.65166812 | s | Client load request to ready; file warming is separately reported, N=1, not a repeated load benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-prefill-current-09/identity.json |
| weight-warming / engine-load | NOT MEASURED | s | Explicit one-byte-per-page warming before load; null when not selected; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-prefill-current-09/identity.json |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| prefill.promessi-8192/sample-0 | PASS | First fresh session completed 8192 new prefill positions and 16 output tokens; isolated observation, not a repeated qualification series |
| prefill.promessi-8192/sample-1 | FAIL | After 512 committed prompt positions, deferred CUDA MoE layer 4 reported device status 1; no generated tokens; first invalid computational owner not yet localized |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Failed repeat retained; fail-closed numerical refusal and scoped host/session cleanup are not successful 8K qualification;  |
| checkpoint-reference | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |
| deployment-performance | UNQUALIFIED | Failed repeat retained; fail-closed numerical refusal and scoped host/session cleanup are not successful 8K qualification;  |
| family-conformance | UNQUALIFIED | Explicit engineering configuration measured through native protocol v24, not ordinary product defaults or complete model qualification;  |
| product-path | UNQUALIFIED | Failed repeat retained; fail-closed numerical refusal and scoped host/session cleanup are not successful 8K qualification;  |
| representation-quality | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |

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
| source_commit | 803dd98d4c54d7a26cb3c08def6b350be51b7179 |
| source_tree | 4ae815ad1669b4c95c302bb9b776e3671ee8f0a4 |
| source_delta | f0fedb271f549473263810ba403ca1a9c4652b3ad43df86a210df21bdf1d9754 |
| build | 6ece97e98ac222ef46ac24bc437d95edf728b4e39194850ee59764058ebaf136 |
| executable | af0bb3a77a62ef2b77280f7aa4d4f4c6edc0265b813533764915a7a589e65dae |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 07ba1f8462465cdaf948a34045aee1727de66dd0942dfb836c3d408d5bd3f69c |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine/native-v24 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not YVEX-published qualification or a model-quality claim.
- Host source/build/checkpoint/hardware provenance unavailable; unknown fields refuse comparison.
- First fragment publication and reasoning-to-final transition not measured.
- Different generated histories are separate input groups, never pooled or treated as deterministic agreement.
- Advisory locks do not prove hardware exclusivity; external resource observation is required.
- No independent oracle; source stability unknown; first sample is not relabeled warmed by hidden work.
- Controlled-engine authority using native protocol v24: explicit target-only, chunk 512, fresh sessions, temperature 0. Not the restored ordinary speculative product configuration.
- Base commit is accompanied by the frozen dirty delta and exact captured executable. This is not a clean published-source qualification.
- File-page cache state is observed separately from mapping/addressability/device allocation. A already-resident page-touch arm is not a cold-prefetch causal experiment.
- Backend/kernel/runtime-toolkit identities unavailable in this import remain null; no passing timing capture promotes numerical or quality qualification.
- A separate 8K repeat on this source/executable reported invalid/non-finite CUDA MoE status; candidate-wide correctness and publication are not earned.
- No 8K rate, median or dispersion is published from a partly failed series. This is an internal numerical investigation, not an external asset blocker.
- Layer 4 and status 1 identify deferred downstream reporting only; graph/workspace reuse remains a hypothesis, not a proven root cause.
