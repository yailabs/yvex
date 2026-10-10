<!-- docs:metadata
title: "0731 Q2_K prepared candidate complete-model CUDA critical-path diagnostic; not a throughput sample"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-prepared-critical-path-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-prepared-critical-path-20261010.json
-->

# 0731 Q2_K prepared candidate complete-model CUDA critical-path diagnostic; not a throughput sample

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-prepared-critical-path-20261010.json)

Target identity: `91324dfe22d0cfde7ea7c06e2d29dd7db719aeda61c3ec804dfc6700079dcbeb`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| gpu.kernel-sum / prefill.promessi-2048 | 23.1158486 | s | Sum of kernel durations; not an additive wall-time decomposition when overlap exists; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.kernel-union / prefill.promessi-2048 | 23.1148714 | s | Union of observed kernel intervals, without double counting overlap; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.kernel-span / prefill.promessi-2048 | 25.4840911 | s | First observed kernel start to final kernel end; excludes unobserved request phases; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_tensorcore.duration / prefill.promessi-2048 | 5.17142654 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_tensorcore_wide_rows.duration / prefill.promessi-2048 | 2.6895149 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_tensorcore_wide_rows.launches / prefill.promessi-2048 | 600 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_matvec.duration / prefill.promessi-2048 | 2.39416167 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_matvec.launches / prefill.promessi-2048 | 10068 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_reduce_native_warp.duration / prefill.promessi-2048 | 2.31234606 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_reduce_native_warp.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_tensorcore.duration / prefill.promessi-2048 | 1.56205257 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_decoded_rows.duration / prefill.promessi-2048 | 1.45063725 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_decoded_rows.launches / prefill.promessi-2048 | 580 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_prepare_digits.duration / prefill.promessi-2048 | 1.33919164 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_prepare_digits.launches / prefill.promessi-2048 | 344 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_rows.duration / prefill.promessi-2048 | 0.941875639 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_rows.launches / prefill.promessi-2048 | 860 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_yarn_rope.duration / prefill.promessi-2048 | 0.767815542 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_yarn_rope.launches / prefill.promessi-2048 | 3416 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_candidate_scores.duration / prefill.promessi-2048 | 0.727307991 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_candidate_scores.launches / prefill.promessi-2048 | 1680 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/cupti.tsv |
| candidate.host-workspace-capacity / prefill.promessi-2048 | 1.59634242e+09 | byte | Prepared candidate host staging capacity, not total physical memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/result.json |
| reference.host-workspace-capacity / prefill.promessi-2048 | 402007272 | byte | Row-matrix reference host staging capacity, not total physical memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-row-matrix-prefill.promessi-2048/result.json |
| candidate.profiled-prefill / prefill.promessi-2048 | 22.4542842 | s | One instrumented complete-model prefill; diagnostic, not a timed performance sample; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prepared-prefill.promessi-2048/result.json |
| reference.profiled-prefill / prefill.promessi-2048 | 19.8731066 | s | One instrumented reference prefill; diagnostic, not a timed performance sample; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-row-matrix-prefill.promessi-2048/result.json |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| prefill.promessi-2048 | FAIL | Full-model execution completed but prepared candidate continuation differs from its same-artifact numerical-class reference; candidate rejected |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Native emission and pinned independent GGUF reader establish structure, not checkpoint-matched quality; Independent held-out comparison missing |
| backend-execution | UNQUALIFIED | Rejected equivalent-layout experiment: component bitwise controls and memcheck pass, but complete-model continuation differs; Full-model numerical divergence not localized; no equivalence claim or retained implementation |
| deployment-performance | CHARACTERIZED | Profiled kernel-population attribution only; diagnostic wall time is not a comparable throughput sample;  |
| product-path | UNQUALIFIED | Direct native engineering entrypoint; no native-protocol/HTTP/client-visible timing in this record;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | 82a4b9ac5e72f5068461f9c5c1efa7b2264fc183c3da1b8ed5bc9901d7fea6df |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 1e1d143a28c02e50a29deb8aa1123eb4d16866e8e2d348aa1d7081f1a9e18da2 |
| representation | goal-v1-q2_k; family-preserved roles unchanged |
| artifact_set | 7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f |
| binding | a4bb99c92e1f8d1fb61f7db3ed0fc5d7a900fcc87b7ecd6fd8c75c486a939737 |
| specialization | e9ff286bc109691f797a38f064d31a04719be9b6257a01613d2581a9bc25233d |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 96685073a18e8035587c11ba2d686b4cb65a5ab7cd3f36b9ad879cd9ecd895bd |
| build | 49330071df376cd22debf08ceec01c6fe706b787f209b7236e3b5d66811da625 |
| executable | 55ac4f3fb49917c7b4ca9b95a26e01139deac784799a26ada2a53ac180943fa3 |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; rejected prepared Q2 candidate |
| kernel_bundle | 86859ed890d2acca517878b5e55f602b094a4348e3a73d6d854ae5008952100d |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated first generation after new engine load; no prefix reuse; source-stable candidate, not installed Host; CUPTI kernel/API activity diagnostic |
| context | 16384 |
| prefill_geometry | chunk 512; actual new positions retained in diagnostics |
| sequence_geometry | requested width 1; fresh isolated execution |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature 1; no stochastic draws |
| product_path | direct native engineering generation; not resident-host/native-protocol performance |
| suite | deepseek-0731-prefill@2026-10-09.1 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One first-post-load sample: no dispersion or sustained qualification gate from this record.
- Target-only none only; reasoning high/maximum, speculation and hosted product are separate evidence.
- Independent checkpoint continuation, held-out quantization quality and the 20/700 performance exit remain unearned.
- The internal subsequent_decode phase excludes readout/sampling; no committed decode rate is derived from it.
- Attention/MoE host phase spans overlap and must not be summed as disjoint critical-path costs.
- Source capture includes six retained CUDA candidate edits; this is not a clean published or installed build.
- Full-model admission does not make the representation a qualified planner recommendation.
- Installed Host/listeners remained unchanged and empty; no installed model generation was created.
- CUPTI diagnostic is separate from non-profiled performance samples; source/build and observer hashes are retained.
- Kernel durations are not additive with CPU API waits or overlapping stages.
- qtype_matvec shape attribution to shared experts is a source/geometry inference tested by the row-matrix intervention; the trace has no tensor-role arguments.
- No memory-bandwidth or occupancy claim is inferred from these duration counts.
- Preparation increases measured diagnostic cost and staging capacity; full-model continuation differs. Rejected before finalist timing; no claimed numerical equivalence, performance benefit or hardware ceiling.
- Experimental source/binaries and full receipts retained outside Git; three experimental source files restored exactly to the prior captured row-matrix implementation.
