<!-- docs:metadata
title: "Program P: ordinary CLI artifact production, independent reader and binding"
id: yvex.evaluation.qualification.mamba-program-p-cli-production-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/mamba-program-p-cli-production-20261010.json
-->

# Program P: ordinary CLI artifact production, independent reader and binding

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/mamba-program-p-cli-production-20261010.json)

Target identity: `b54a1331d502665e1d12ee201739d5a31875a955f9aa63aa9d421f3011c96b94`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| production.file-bytes / source-preserving-production | 1.45744911e+10 | byte | Exact emitted file extent, independently structurally read; not runtime working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/mamba-cli-production/result.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model execution in this production control;  |
| checkpoint-reference | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| representation-quality | BLOCKED | Container proofs are not independent full-model quality evidence; Independent full-model evidence not supplied by this control |
| backend-execution | UNQUALIFIED | No model execution in this production control;  |
| deployment-performance | UNQUALIFIED | No model execution in this production control;  |
| product-path | CHARACTERIZED | Ordinary Rust CLI emits a new source-preserving Mamba artifact, reads its GGUF structure independently and publishes the authenticated binding; overwrite and late-binding-refusal controls preserve the existing artifact.;  |

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
| source_delta | 4971f1f5d4de0f02d44ed8f0d09f7e14e391c0ba697c43c1e123d73c99b1a9eb |
| build | e0c46a69eff1fed162ce4f41ac16ba445087fa9018997ded8e762220f724dba4 |
| executable | b0a1f44f7462b9104422135d71baeb300c83eea66c5e6d4388b76165ecfb0ecc |
| backend | cpu |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark; Arm CPU execution only |
| device_count | 1 |
| topology | single coherent CPU memory domain; GPU unused |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | 130663165952 system bytes; offline source-to-artifact production; no engine residency |
| runtime_configuration | offline emission and binding only; no resident engine |
| context | NOT RETAINED |
| prefill_geometry | NOT RETAINED |
| sequence_geometry | NOT RETAINED |
| concurrency | NOT RETAINED |
| strategy | not invoked |
| reasoning | not invoked |
| sampling | not invoked |
| product_path | Rust CLI compile quant emit --binding-directory; no inference transport |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No inference, throughput, quantization-quality or hosted-conversation claim.
- The pinned independent GGUF reader establishes structural facts, not checkpoint-specific numerical quality.
- This is the opt-in emit-to-binding path. Goal-driven final-candidate evaluation and qualified recommendation remain incomplete.
- Source stability binds the retained dirty delta and executable; the HEAD alone does not identify this implementation.
- A late binding refusal leaves the complete artifact intact and exits nonzero. It does not roll back publication or load an engine.
- Linux AArch64 consumer evidence only; this control does not qualify the new external reader build on Darwin.
- Installed Host, management service and models were not changed.
