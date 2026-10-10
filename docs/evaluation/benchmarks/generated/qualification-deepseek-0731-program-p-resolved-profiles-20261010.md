<!-- docs:metadata
title: "DeepSeek 0731: bounded role allocation and canonical policy/plan production"
id: yvex.evaluation.qualification.deepseek-0731-program-p-resolved-profiles-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-resolved-profiles-20261010.json
-->

# DeepSeek 0731: bounded role allocation and canonical policy/plan production

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-resolved-profiles-20261010.json)

Target identity: `7ce6d69d51f5742f4367651662315e68b88c896add497b1e796c25c0ebe8e37a`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| compiler.candidates / 0731-allocation | 26 | count | Actual compiled candidate population; not qualified configurations; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |
| compiler.encoded-bytes / 0731-allocation | 1.04916221e+11 | byte | Canonical planned encoded tensor bytes; not physical working memory or an emitted artifact; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |
| compiler.initial-lower-bound / 0731-allocation | 1.22308178e+11 | byte | Weights plus largest tensor and reserve; explicitly excludes unknown full workspace/state; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |
| compiler.available-bytes / 0731-allocation | 1.26285468e+11 | byte | Sampled available system memory; not a resource reservation or exclusion of the resident operator engine; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |
| model.attention-layers / 0731-allocation | 43 | count | Source-authenticated semantic profile, not a Transformer assumption; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |
| model.sequence-mixer-layers / 0731-allocation | 0 | count | Source-authenticated semantic profile; real state-space topology; /home/dgmothx/lab/models/evidence/physical-profiles-20261010.8cRfaJF3/final-controls/0731-allocation.stdout |

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
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 8aa1c45dc98828099aea215b485f5eaa93fb0400805d8ad50e9b39c76b85e84a |
| representation | 5d3bea3529b2c690b982d2c32c6aa3a4e2130b0cc90cd0416ad64dbf146002c4 |
| artifact_set | NOT RETAINED |
| binding | NOT RETAINED |
| specialization | NOT RETAINED |
| source_commit | 75fe822070c48ad3c1ab59cb1ed5f9044ac66bbd |
| source_tree | 035b546897c2c6ce75edef3b7cd15f59b0d77ca3 |
| source_delta | 46798a7a2756bfa7b61c4db90bf90c25544d2ba13b1ec092aa359d82789a1335 |
| build | c39a1a67d8697b7e20cc8918c37442b6514cbc2471ae0f13987e027ac9a9e28d |
| executable | a3dfd7ae9f48b60d1b1f2a9f0992db0f7798b22d16536ef4d81ec40bf26fbaa9 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory; compiler inspection only |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
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
- The exported source-retention-v1-0000000000000023 recipe is an experiment, not a performance recommendation. One source-preserved attention tensor passes an internal probe; no logits/NLL/PPL/KL or held-out score is provided.
