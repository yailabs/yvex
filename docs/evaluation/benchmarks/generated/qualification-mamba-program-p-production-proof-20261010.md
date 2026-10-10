<!-- docs:metadata
title: "Program P: new Mamba artifact admitted through generic production proof"
id: yvex.evaluation.qualification.mamba-program-p-production-proof-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/mamba-program-p-production-proof-20261010.json
-->

# Program P: new Mamba artifact admitted through generic production proof

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/mamba-program-p-production-proof-20261010.json)

Target identity: `4d9777501455385462d876aeac0a5d24d972afde712db6bf16854c7a11e0c99f`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| production.file-bytes / source-preserving-production | 1.45744911e+10 | byte | Exact published artifact extent independently read; not working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/production-bridge-directory-result.log |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model execution in this production control;  |
| checkpoint-reference | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| representation-quality | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| backend-execution | UNQUALIFIED | No model execution in this production control;  |
| deployment-performance | UNQUALIFIED | No model execution in this production control;  |
| product-path | CHARACTERIZED | Native compiler API admits a fresh noncatalog artifact using complete production proof; missing reader refuses without binding publication;  |

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
| source_delta | 1ff446bafc4478104e99101962d5bbb530062e6b83ab7f32347bc31d4aaf0ff0 |
| build | static-library-sha256:575a2efe5132fe0b9508a151395ece57ec1d7c2f7f675f1ae2aa5897d2330e4c |
| executable | c5af3df90edb0a84481ce09fabe5f10373ca1aa5495f483c67196981eb62b76a |
| backend | cpu |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark; Arm CPU execution only |
| device_count | 1 |
| topology | single coherent CPU memory domain; GPU unused |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | 130663165952 system bytes; mapped BF16 artifact; separate recurrent state banks |
| runtime_configuration | offline emission and binding only; no resident engine |
| context | NOT RETAINED |
| prefill_geometry | NOT RETAINED |
| sequence_geometry | NOT RETAINED |
| concurrency | NOT RETAINED |
| strategy | not invoked |
| reasoning | not invoked |
| sampling | not invoked |
| product_path | native compiler C API integration fixture; not CLI inference |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No inference, performance, numerical quality or hosted conversation claim.
- Pinned independent reader proves GGUF structure and counts, not model meaning.
- Borrowed complete production proof is internal API; public receipt import and automatic optimization remain incomplete.
- An earlier harness omitted the destination directory and correctly failed publication; repaired harness creates only its owned output directory.
- Installed Host and its services were not modified.
