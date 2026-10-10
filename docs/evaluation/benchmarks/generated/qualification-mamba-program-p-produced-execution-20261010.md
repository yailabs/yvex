<!-- docs:metadata
title: "Program P second architecture: newly produced Mamba BF16 artifact, bounded native CPU execution and state controls"
id: yvex.evaluation.qualification.mamba-program-p-produced-execution-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/mamba-program-p-produced-execution-20261010.json
-->

# Program P second architecture: newly produced Mamba BF16 artifact, bounded native CPU execution and state controls

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/mamba-program-p-produced-execution-20261010.json)

Target identity: `b9814d2974213785571f896b6b59d66245a6e7d184cef69e682f098fe32f54c0`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| runtime.mapped_model_bytes / bounded-candidate-readout | 1.45744911e+10 | byte | Native owned extent, not observed physical RSS or additive device memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-new-production-execution-v2/result.log |
| runtime.prepared_model_bytes / bounded-candidate-readout | 0 | byte | Native owned extent, not observed physical RSS or additive device memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-new-production-execution-v2/result.log |
| runtime.shared_state_bytes / bounded-candidate-readout | 557842432 | byte | Native owned extent, not observed physical RSS or additive device memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-new-production-execution-v2/result.log |
| runtime.branch_state_bytes / bounded-candidate-readout | 557842432 | byte | Native owned extent, not observed physical RSS or additive device memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-new-production-execution-v2/result.log |
| runtime.workspace_bytes / bounded-candidate-readout | 0 | byte | Native owned extent, not observed physical RSS or additive device memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-new-production-execution-v2/result.log |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model execution in this production control;  |
| checkpoint-reference | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| representation-quality | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| backend-execution | CHARACTERIZED | Fresh noncatalog artifact passes actual native execution, ordering, cancellation, no partial publication, replay and cleanup controls; not independent model quality; logsumexp max absolute error 3.14446848770612795185e-15 <= 1e-12; internal replay 9.7856314422183977e-15 <= 1e-12;  |
| deployment-performance | UNQUALIFIED | One correctness execution with concurrent software work; no timed performance claim;  |
| product-path | CHARACTERIZED | Fresh noncatalog artifact passes actual native execution, ordering, cancellation, no partial publication, replay and cleanup controls; not independent model quality;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | mamba2 |
| upstream_repository | mistralai/Mamba-Codestral-7B-v0.1 |
| checkpoint | 4f086c08c1e0f07bdc50ca25125dbbf7475d21da |
| tokenizer_conversation | 2555e53b19079a3037a9449265252d1c9bdee6bcfb2063f24e62386db1b52ea4 |
| transformation_ir | 6cde84173a2cc012d1b3c475d971a45a27d5506019c95cc09ffa0aa7a4ceab08 |
| physical_policy | 493f51f533cf81fe4c3c1ceeef1a23f787c7dc509ca4787687708c6e6dfe615b |
| representation | goal-v1-source; family-preserved BF16; physical variant 45fcb2cff55c08df9a999e401bf9071cfbd03e524966ddabbf5458af1569a8fe |
| artifact_set | 6038bca5b2c7e56c6e347019b60612ebf35dee5f771ff67941d2e1babaf673d3 |
| binding | bee48031b8100b3f76f35e74fe819ab7d728b11a4e0e4adcdf907963ce3bb938 |
| specialization | NOT RETAINED |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 32b0b37da4b7e3b96013dbe7600b52afa42faa483b1e0715dca379984dd8809a |
| build | static-library-sha256:02f51698cf2b22af705464610b78574ce31b285e94c350e0c824bcf9e37e09a9 |
| executable | 5b71c9d9a99bae75d9999661a52010fb6895ea3b57f54ac87bca88cb44b384d3 |
| backend | cpu |
| backend_implementation | portable CPU reference; recurrent Mamba execution |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark; Arm CPU execution only |
| device_count | 1 |
| topology | single coherent CPU memory domain; GPU unused |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | 130663165952 system bytes; mapped BF16 artifact; separate recurrent state banks |
| runtime_configuration | bounded native readout fixture; independent candidate state and shared prefix; no installed service |
| context | 8 |
| prefill_geometry | one prefix token |
| sequence_geometry | four candidate branches; five teacher-forced tokens |
| concurrency | 1 |
| strategy | finite candidate scoring; no generation |
| reasoning | not invoked |
| sampling | not invoked |
| product_path | native computational C API fixture; not CLI chat, public finite producer or HTTPS |
| suite | registered live.mamba2.decision-readout fixture; exact source digest retained |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No independent checkpoint logits or representation-quality score.
- Source-preserving BF16 production and real CPU execution do not establish CUDA or chat support.
- The installed Host, finite service and operator generations were not loaded, replaced or requalified.
- No performance measurement: concurrent source builds and one bounded correctness run.
- Planner-to-production-to-binding-to-execution reuse is established at native owners; automatic qualified recommendation remains incomplete.
