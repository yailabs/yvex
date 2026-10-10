<!-- docs:metadata
title: "Program P: canonical Mamba recipe and binding execute bounded token controls"
id: yvex.evaluation.qualification.mamba-program-p-reuse-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/mamba-program-p-reuse-20261010.json
-->

# Program P: canonical Mamba recipe and binding execute bounded token controls

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/mamba-program-p-reuse-20261010.json)

Target identity: `4ff21c8e090cb0e54fe78b5cbba62bbcf23e9b22ca3a35785a7bc505020d7b85`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Existing family conformance not rerun by this compiler reuse control;  |
| checkpoint-reference | BLOCKED | Replay uses YVEX forward logits, not an independent whole-model implementation; Independent whole-model reference not supplied |
| representation-quality | BLOCKED | Canonical BF16 artifact preserved; no new quality score; Independent whole-model quality remains unearned |
| backend-execution | CHARACTERIZED | Compiler-selected canonical recipe reproduces exact binding; CPU token execution, order, replay, cancellation and cleanup controls pass;  |
| deployment-performance | UNQUALIFIED | Correctness fixture overlaps CPU artifact compilation; no comparable timing sample;  |
| product-path | UNQUALIFIED | Separate raw generation probe refuses unavailable persistent generation state before output; no chat capability promotion;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | mamba2 |
| upstream_repository | mistralai/Mamba-Codestral-7B-v0.1 |
| checkpoint | 4f086c08c1e0f07bdc50ca25125dbbf7475d21da |
| tokenizer_conversation | 2555e53b19079a3037a9449265252d1c9bdee6bcfb2063f24e62386db1b52ea4 |
| transformation_ir | 6cde84173a2cc012d1b3c475d971a45a27d5506019c95cc09ffa0aa7a4ceab08 |
| physical_policy | 5f457af00c3c47af62f4ee6ddcdcd15292d569857c6515612c79f6847b75dd5c |
| representation | mamba-codestral-source-faithful-v1 |
| artifact_set | bf0053bf02a235563342a0281acfc73e17eb287542a7109e975db414458a77d3 |
| binding | f8eae71cdef043cea3fc0b46264026f65944892c6b1703c7761c3eab61afabb6 |
| specialization | NOT RETAINED |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 8967bfd3c14ac7dcf1a2ac90e2f2fcad3039e670700f0c7058278b54c17a8798 |
| build | cb9434c7b482251a73b5dfcd0877ffe06185e855b9156b8e32971e8f5b153990 |
| executable | 72cc5d1078eb54c3cbb4be38c1e0cfead38a5bff5c2450e45bb53b618791af18 |
| backend | cpu |
| backend_implementation | selective_ssd.cpu.f32state.v1; portable reference |
| kernel_bundle | cb9434c7b482251a73b5dfcd0877ffe06185e855b9156b8e32971e8f5b153990 |
| hardware_model | NVIDIA DGX Spark; Arm CPU execution only |
| device_count | 1 |
| topology | single coherent CPU memory domain; GPU unused |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | 130663165952 system bytes; mapped BF16 artifact; separate recurrent state banks |
| runtime_configuration | isolated readout fixture; one-token prefix; four candidates; no Host enrollment |
| context | 8 |
| prefill_geometry | one prefix position; token width 1 |
| sequence_geometry | one shared prefix; isolated replay controls |
| concurrency | 1 |
| strategy | finite candidate readout; no sampling or generated output |
| reasoning | not applicable; no conversation template |
| sampling | not invoked; teacher-forced supplied token IDs |
| product_path | native C runtime fixture; not CLI chat or HTTP |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Compiler reuse does not establish a Pareto-optimal recipe or qualified recommendation.
- The synthesized source-preserving policy with a different physical identity was correctly refused by the unchanged family artifact catalog.
- This is a token-level runtime control, not hosted text generation, model quality or a performance benchmark.
- The installed Host and finite service were not loaded, restarted or reconfigured.
