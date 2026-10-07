<!-- docs:metadata
title: "DeepSeek independent coding continuation \u2014 high"
id: yvex.evaluation.qualification.deepseek-independent-coding-target-03-high
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-independent-coding-target-03-high.json
-->

# DeepSeek independent coding continuation — high

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-independent-coding-target-03-high.json)

Target identity: `a1014dab5ac6efeb7afecfcdbd44d6fe6ae1ab9f93f54b5b414bb6a5af646523`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| greedy-prefix / coding.metal | 0 | token | 1 | 0–0 | 0 |

## Independent continuation comparison

This is exact-input characterization, not cross-realization numerical equivalence or a performance measurement.

| Case | Exact input tokens | First token | Matching greedy prefix | Reference / candidate sampled | Candidate committed | Exact bounded continuation |
| --- | --- | --- | ---: | ---: | ---: | --- |
| coding.metal | 53 | DIFFERS | 0 | 256 / 256 | 256 | DIFFERS |

Independent implementation: **llama.cpp-deepseek-v4-flash** @ `2f2d44052b7d15c9c4dd6610f6e14a5f7b2d5f3f`; executable `df2fee12b815701ebaa2eaaf0d840fdda26a98b83a2f14f47a2249d5ea254446`.
Full distributions, teacher-forced NLL/PPL, KL and RMS probability delta: **NOT MEASURED**.


## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Independent continuation comparison does not run the family conformance gate;  |
| checkpoint-reference | CHARACTERIZED | Exact-input greedy-prefix characterization against the declared independent realization;  |
| representation-quality | BLOCKED | No admitted higher-precision checkpoint or full-distribution comparison/tolerance; No admitted higher-precision checkpoint or full-distribution comparison/tolerance |
| backend-execution | UNQUALIFIED | Bounded completion does not replace backend numerical and lifecycle qualification;  |
| deployment-performance | UNQUALIFIED | No performance claim; preparation and diagnostic work are not resident-host timing;  |
| product-path | UNQUALIFIED | Direct engineering generation, not the native local product or HTTP compatibility path;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek4-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | artifact-sha256:b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | NOT RETAINED |
| specialization | NOT RETAINED |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | e04488c6b373c76ec3b19fae80a922fdc428010f0a88987046ff26ecd6ab3681 |
| build | a35918dd0939fdcb1da3a383bb7c6fb6ecb0c7e574acde4cd70c27a7dc80d39a |
| executable | cc5781e1a137eeeef031a360d5fd39cb7d136521baa67b869473f2fa522db53d |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | bd257030270b4231297b1a7d2390fa0d52bde02fc036267a85e3b6ea93d9eb0f |
| context | 4096 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | high |
| sampling | {"min_p":0,"seed_present":false,"stochastic":false,"strategy":"greedy","temperature":1,"top_k":0,"top_p":1,"typical_p":1} |
| product_path | controlled-engine |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **greedy-prefix**: matching continuation prefix under the declared exact comparison policy. Scope: Exact-input greedy-prefix characterization against the declared independent realization. Session: fresh; warm/cold: not a timing measurement; standalone generation includes preparation; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-reference-comparison-20261005.bMoZ3v/coding-target-03/candidate-1.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Representative suite inputs are not official upstream inference vectors; the four official encoding cases remain a separate gate.
- Exact-input free-running continuations use the declared independent arithmetic/weight/KV realization; no cross-realization tolerance is invented.
- Only positions before first divergence share a causal context; later token equality is not teacher-forced same-top-token accuracy.
- Missing full logits, teacher-forced NLL/PPL, KL and probability deltas are unavailable, never zero error.
- One bounded observation per case is not a statistical quality guarantee, model-quality score or sustained-throughput result.
- Standalone preparation/teardown is not warm product performance; native and HTTP timings require their own targets.
- Historical unprojected plan identities remain null; neither newer source nor another receipt fills them retroactively.
