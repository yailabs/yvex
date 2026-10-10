<!-- docs:metadata
title: "0731 Q2_K complete-model CUDA critical-path diagnostic; not a throughput sample"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-critical-path-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-critical-path-20261010.json
-->

# 0731 Q2_K complete-model CUDA critical-path diagnostic; not a throughput sample

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-critical-path-20261010.json)

Target identity: `6f8fc68b8e0c9758b1757c83bed6ffda538f267f2acc87866253225b69096f72`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| gpu.kernel-sum / prefill.promessi-2048 | 26.7504412 | s | Sum of kernel durations; not an additive wall-time decomposition when overlap exists; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.kernel-union / prefill.promessi-2048 | 26.7497743 | s | Union of observed kernel intervals, without double counting overlap; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.kernel-span / prefill.promessi-2048 | 28.7403707 | s | First observed kernel start to final kernel end; excludes unobserved request phases; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_matvec.duration / prefill.promessi-2048 | 9.14155222 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_matvec.launches / prefill.promessi-2048 | 10584 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_tensorcore.duration / prefill.promessi-2048 | 4.02398564 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_tensorcore_wide_rows.duration / prefill.promessi-2048 | 2.696141 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_qtype_tensorcore_wide_rows.launches / prefill.promessi-2048 | 600 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_reduce_native_warp.duration / prefill.promessi-2048 | 2.32093827 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_reduce_native_warp.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_tensorcore.duration / prefill.promessi-2048 | 1.59150502 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_decoded_rows.duration / prefill.promessi-2048 | 1.46803795 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_decoded_rows.launches / prefill.promessi-2048 | 580 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_rows.duration / prefill.promessi-2048 | 0.851407673 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_up_rows.launches / prefill.promessi-2048 | 860 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_yarn_rope.duration / prefill.promessi-2048 | 0.769204903 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_yarn_rope.launches / prefill.promessi-2048 | 3416 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_candidate_scores.duration / prefill.promessi-2048 | 0.736375563 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_attention_candidate_scores.launches / prefill.promessi-2048 | 1680 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_rows.duration / prefill.promessi-2048 | 0.587358869 | s | Observed kernel population total; kernel names alone do not identify tensor qtype or semantic role; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |
| gpu.yvex_moe_grouped_down_rows.launches / prefill.promessi-2048 | 860 | count | Observed kernel population launches; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-prefill.promessi-2048/cupti.tsv |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| prefill.promessi-2048 | PASS | Completed instrumented model execution; not a timed performance sample |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Native emission and pinned independent GGUF reader establish structure, not checkpoint-matched quality; Independent held-out comparison missing |
| backend-execution | CHARACTERIZED | Profiled kernel-population attribution only; diagnostic wall time is not a comparable throughput sample;  |
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
| specialization | 2a72a31a58bf55f39d74221c27cf80f1348c02b143d8b7f028d7c3a8a523820e |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 0954ac9646d5d4a98dd2e460e7656c995ef27e0b1ad9e31548d1c03a056601c4 |
| build | 43b7e711cd984715151d7841f65fbd8270cf0e00589862a5bfb0d58fc0d08421 |
| executable | 9e4f3ea7c4f9b1fb942774117bfa6df0e405989d3a680fadc5471f1425c3b291 |
| backend | cuda |
| backend_implementation | CUDA homogeneous Q2_K routed matrix + encoded row; declared existing arithmetic |
| kernel_bundle | 4a21492217e459a8c6391d6dfd068d8154f704186249bbb64e67a5cba173a6ad |
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
