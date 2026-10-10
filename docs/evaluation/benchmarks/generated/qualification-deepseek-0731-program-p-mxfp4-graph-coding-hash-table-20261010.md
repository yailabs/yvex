<!-- docs:metadata
title: "0731 MXFP4/Q2 complete-model projection geometry including CUDA graph replay: coding.hash-table"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mxfp4-graph-coding-hash-table-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mxfp4-graph-coding-hash-table-20261010.json
-->

# 0731 MXFP4/Q2 complete-model projection geometry including CUDA graph replay: coding.hash-table

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mxfp4-graph-coding-hash-table-20261010.json)

Target identity: `d854c68fa7bc158861925f36a7c88915e5fd8243ca97cf482df5257b7dd25c4d`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| projection.0.duration / coding.hash-table | 0.054074764 | s | width=2048 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.0.launches / coding.hash-table | 1376 | count | width=2048 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.1.duration / coding.hash-table | 0.014725081 | s | width=4096 rows=64 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.1.launches / coding.hash-table | 672 | count | width=4096 rows=64 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.2.duration / coding.hash-table | 0.001354915 | s | width=4096 rows=64 inputs=31 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.2.launches / coding.hash-table | 21 | count | width=4096 rows=64 inputs=31 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.3.duration / coding.hash-table | 0.07978243 | s | width=4096 rows=256 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.3.launches / coding.hash-table | 2720 | count | width=4096 rows=256 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.4.duration / coding.hash-table | 0.012138389 | s | width=4096 rows=256 inputs=31 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.4.launches / coding.hash-table | 85 | count | width=4096 rows=256 inputs=31 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.5.duration / coding.hash-table | 0.071438056 | s | width=4096 rows=512 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.5.launches / coding.hash-table | 1280 | count | width=4096 rows=512 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.6.duration / coding.hash-table | 0.021451922 | s | width=4096 rows=512 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.6.launches / coding.hash-table | 1376 | count | width=4096 rows=512 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.7.duration / coding.hash-table | 0.010515734 | s | width=4096 rows=512 inputs=31 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.7.launches / coding.hash-table | 40 | count | width=4096 rows=512 inputs=31 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.8.duration / coding.hash-table | 0.100925515 | s | width=4096 rows=1024 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.8.launches / coding.hash-table | 1344 | count | width=4096 rows=1024 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.9.duration / coding.hash-table | 0.032153279 | s | width=4096 rows=1024 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.9.launches / coding.hash-table | 1376 | count | width=4096 rows=1024 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.10.duration / coding.hash-table | 0.02013047 | s | width=4096 rows=1024 inputs=31 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.10.launches / coding.hash-table | 42 | count | width=4096 rows=1024 inputs=31 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.11.duration / coding.hash-table | 0.135460518 | s | width=4096 rows=2048 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.11.launches / coding.hash-table | 2752 | count | width=4096 rows=2048 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.12.duration / coding.hash-table | 0.202125749 | s | width=4096 rows=129280 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.12.launches / coding.hash-table | 32 | count | width=4096 rows=129280 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.13.duration / coding.hash-table | 0.183293169 | s | width=8192 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.13.launches / coding.hash-table | 1376 | count | width=8192 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.14.duration / coding.hash-table | 0.091732076 | s | width=16384 rows=24 inputs=1 qtype=0 q8=0 block=1 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.14.launches / coding.hash-table | 2752 | count | width=16384 rows=24 inputs=1 qtype=0 q8=0 block=1 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.15.duration / coding.hash-table | 0.181510811 | s | width=16384 rows=24 inputs=31 qtype=0 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| projection.15.launches / coding.hash-table | 86 | count | width=16384 rows=24 inputs=31 qtype=0 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_qtype_matvec.duration / coding.hash-table | 1.21281288 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_qtype_matvec.launches / coding.hash-table | 17330 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_up_rows.duration / coding.hash-table | 0.435995641 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_up_rows.launches / coding.hash-table | 1419 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_down_rows.duration / coding.hash-table | 0.277385904 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_down_rows.launches / coding.hash-table | 1419 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_decoded_mxfp4_rows.duration / coding.hash-table | 0.243137046 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_decoded_mxfp4_rows.launches / coding.hash-table | 1376 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_mxfp4_q8_rows.duration / coding.hash-table | 0.217181901 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_mxfp4_q8_rows.launches / coding.hash-table | 2048 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_residual_mhc_pre.duration / coding.hash-table | 0.190687678 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_residual_mhc_pre.launches / coding.hash-table | 2838 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_yarn_rope.duration / coding.hash-table | 0.104093902 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_yarn_rope.launches / coding.hash-table | 5328 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_qtype_tensorcore_rows.duration / coding.hash-table | 0.086903659 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_qtype_tensorcore_rows.launches / coding.hash-table | 236 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_reduce_native.duration / coding.hash-table | 0.070311155 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_reduce_native.launches / coding.hash-table | 1376 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_activation_quantize.duration / coding.hash-table | 0.070108916 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_activation_quantize.launches / coding.hash-table | 2490 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_weighted_norm.duration / coding.hash-table | 0.066847059 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_attention_weighted_norm.launches / coding.hash-table | 6054 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_up_tensorcore.duration / coding.hash-table | 0.0547171 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| kernel.yvex_moe_grouped_up_tensorcore.launches / coding.hash-table | 43 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/cupti.tsv |
| runtime.accelerated_matrix_launches / coding.hash-table | 322 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.accepted_draft_tokens / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.cache_evictions / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.cache_hits / coding.hash-table | 5676 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.cache_misses / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.d2d_bytes / coding.hash-table | 2.19729767e+09 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.d2h_bytes / coding.hash-table | 409761132 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.device_allocations / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.device_synchronizations / coding.hash-table | 33 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.discarded_candidate_rows / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.downloads / coding.hash-table | 4290 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.draft_forwards / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.draft_rows / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.event_synchronizations / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.expert_bytes / coding.hash-table | 1.38307182e+11 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.expert_subviews / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.full_array_host_scan_bytes / coding.hash-table | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.generated_tokens / coding.hash-table | 32 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.graph_captures / coding.hash-table | 235 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.graph_launches / coding.hash-table | 2838 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.graph_replays / coding.hash-table | 2838 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.h2d_bytes / coding.hash-table | 416325372 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.host_allocations / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.host_payload_reads / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.kernel_launches / coding.hash-table | 81049 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.logits_d2d_bytes / coding.hash-table | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.logits_d2h_bytes / coding.hash-table | 768 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.logits_h2d_bytes / coding.hash-table | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.managed_prefetch_bytes / coding.hash-table | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.mapped_bytes_touched / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.new_prefill_tokens / coding.hash-table | 31 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.output_head_rows / coding.hash-table | 32 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.promoted_target_rows / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.prompt_tokens / coding.hash-table | 31 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.queue_synchronizations / coding.hash-table | 4355 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.replayed_accepted_target_rows / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.reused_tokens / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.row_expert_pairs / coding.hash-table | 16254 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.target_extensions / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.target_forwards / coding.hash-table | 33 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.target_rows / coding.hash-table | 63 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.target_verifications / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.unique_experts / coding.hash-table | 10890 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.uploads / coding.hash-table | 1452 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.verified_rows / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |
| runtime.workspace_resets / coding.hash-table | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-coding.hash-table/result.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No model inference or quality claim from artifact admission and load;  |
| checkpoint-reference | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| representation-quality | BLOCKED | No model inference or quality claim from artifact admission and load; Independent checkpoint-matched quality evidence missing |
| backend-execution | CHARACTERIZED | Instrumented complete model; physical projection geometry and kernel intervals, not timed throughput or a numerical oracle;  |
| deployment-performance | CHARACTERIZED | Instrumented complete model; physical projection geometry and kernel intervals, not timed throughput or a numerical oracle;  |
| product-path | UNQUALIFIED | No native socket or HTTP consumer in this diagnostic;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 1af3500d0025db9b262b42013b3105c5dc0d6fb49276ecb9c86fe5d484d3ad93 |
| representation | goal-v1-mxfp4-routed-q2_k; physical variant 572c0689fdc28e093656ba6986f18d1410a85891c690c3acad98f0d9269c4b8a |
| artifact_set | 25d29155aa86505ce0aa5a1c5535f9ed6e3b073d1803409a646b1f643423f041 |
| binding | 091f933e11d8dd129e86d186f622c20f637ffe98b23d2d78fe118bd1a2199632 |
| specialization | 87d7a08ec2dfb0b9a9f2861a57455472ffccf1089e433db5eda543cd824dc13e |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 516b569f2204993ac54a4a1a61535b8cf26781ede78fe8d9a7b8995abd4d1072 |
| build | 407b1df74781f1d78b0459f94f874475ce2ae5667552e668646524466ff16a1e |
| executable | 4507f4f3ed6a4e10e92c04b6b2975c189ea48f4630d51f33b789f15aaa98862a |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | 43eb78da7f8beb3edfda0ee3cf12e18cc17db96808cafaba4b7d6d192ee4cea1 |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | fresh headless execution; kernel/API and scalar launch/graph-node observer |
| context | 4096 |
| prefill_geometry | chunk 512; newly executed 31 positions |
| sequence_geometry | width 1; fresh state |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature argument 1 inactive under greedy selection |
| product_path | controlled-engine native benchmark; CUPTI-instrumented; not local protocol |
| suite | deepseek-gb10-competitive-v1 / coding.hash-table |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Profiled 32-output coding / 16-output prefill diagnostic, not sustained unprofiled performance or a native-product measurement.
- Geometry is copied from scalar launch arguments and inherited by correlated graph nodes; no weight, activation or hidden reasoning payload is captured.
- No missed projection association or dropped CUPTI activity in these captures; kernel counts alone do not prove occupancy, bandwidth or Tensor Core utilization.
- API waits overlap device activity; stage and kernel sums must not be added to request duration.
- Concurrent software validation/build activity prevents an uncontended host-overhead claim; frozen numerical executable remains unchanged.
- The first graph observer fixture crashed due to an incorrect resource descriptor cast; repaired against installed CUDA sample and qualified independently before this model run.
- Independent representation quality and 20/700 performance gates remain unearned.
