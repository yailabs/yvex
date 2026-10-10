<!-- docs:metadata
title: "Mamba2: native guided profile resolution and explicit technique/backend refusal"
id: yvex.evaluation.qualification.mamba2-program-p-resolved-profiles-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/mamba2-program-p-resolved-profiles-20261010.json
-->

# Mamba2: native guided profile resolution and explicit technique/backend refusal

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/mamba2-program-p-resolved-profiles-20261010.json)

Target identity: `b20506a86a7cf19b371fbd7088de6dac5b3af02e9107a5fa28c94fed6ee73636`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| compiler.candidates / mamba-presets | 7 | count | Actual compiled candidate population; not qualified configurations; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |
| compiler.encoded-bytes / mamba-presets | 1.45708073e+10 | byte | Canonical planned encoded tensor bytes; not physical working memory or an emitted artifact; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |
| compiler.initial-lower-bound / mamba-presets | 3.11721385e+10 | byte | Weights plus largest tensor and reserve; explicitly excludes unknown full workspace/state; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |
| compiler.available-bytes / mamba-presets | 1.25807014e+11 | byte | Sampled available system memory; not a resource reservation or exclusion of the resident operator engine; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |
| model.attention-layers / mamba-presets | 0 | count | Source-authenticated semantic profile, not a Transformer assumption; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |
| model.sequence-mixer-layers / mamba-presets | 64 | count | Source-authenticated semantic profile; real state-space topology; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/mamba-presets.stdout |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not executed by these offline compiler controls;  |
| checkpoint-reference | BLOCKED | Source retention and an internal tensor probe are not independent model quality; Independent checkpoint-matched full-model held-out evidence unavailable |
| representation-quality | BLOCKED | Source retention and an internal tensor probe are not independent model quality; Independent checkpoint-matched full-model held-out evidence unavailable |
| backend-execution | UNQUALIFIED | Not executed by these offline compiler controls;  |
| deployment-performance | UNQUALIFIED | Not executed by these offline compiler controls;  |
| product-path | CHARACTERIZED | Profile resolution, exact planning, declared refusals and reproducible native CLI; no inference;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | mamba2 |
| upstream_repository | mistralai/Mamba-Codestral-7B-v0.1 |
| checkpoint | 4f086c08c1e0f07bdc50ca25125dbbf7475d21da |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | 6cde84173a2cc012d1b3c475d971a45a27d5506019c95cc09ffa0aa7a4ceab08 |
| physical_policy | 5f457af00c3c47af62f4ee6ddcdcd15292d569857c6515612c79f6847b75dd5c |
| representation | a10c94158265f53ae6e6add7a11d1fae3851e3b51d22129d63701e80fdfff0c9 |
| artifact_set | NOT RETAINED |
| binding | NOT RETAINED |
| specialization | NOT RETAINED |
| source_commit | 75fe822070c48ad3c1ab59cb1ed5f9044ac66bbd |
| source_tree | 035b546897c2c6ce75edef3b7cd15f59b0d77ca3 |
| source_delta | 46798a7a2756bfa7b61c4db90bf90c25544d2ba13b1ec092aa359d82789a1335 |
| build | c39a1a67d8697b7e20cc8918c37442b6514cbc2471ae0f13987e027ac9a9e28d |
| executable | a3dfd7ae9f48b60d1b1f2a9f0992db0f7798b22d16536ef4d81ec40bf26fbaa9 |
| backend | cpu |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | DGX Spark aarch64 CPU |
| device_count | 1 |
| topology | single CPU process; no device execution |
| driver | NOT RETAINED |
| runtime_toolkit | native C compiler planning |
| memory_configuration | 130663165952 system bytes; available memory is a separate observation |
| runtime_configuration | offline profile resolution and physical planning; no engine or workspace admission |
| context | 4096 |
| prefill_geometry | requested chunk 512; not executed |
| sequence_geometry | requested width 1; no session |
| concurrency | 1 |
| strategy | planning-only |
| reasoning | not executed |
| sampling | not executed |
| product_path | native Rust compile optimize CLI; not inference |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Planning and source-retained element count are not measured quality, throughput or a qualified recommendation.
- No full artifact/binding emitted by this continuation; no engine loaded, unloaded or replaced.
- Full pre-production workspace/state accounting, automatic authenticated measured selection and complete guided production remain open.
- Six pre-existing experimental CUDA edits are present in the retained source snapshot but neither exercised by model inference nor accepted by this compiler evidence.
- The operator DSpark generation 1 remained resident; no same-machine inference measurement was attempted.
- Software tests and official tokenizer vectors are separate from independent full-model reference and representation quality.
- Mamba source-faithful plans retain BF16; recipe labels do not imply a new quantization. Source-retention allocation refuses because there are no legal alternative groups. Metal candidates are explicitly unsupported; no GPU model capability is inferred.
