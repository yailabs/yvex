<!-- docs:metadata
title: "0731 MXFP4/Q2 complete-model projection geometry including CUDA graph replay: prefill.promessi-2048"
id: yvex.evaluation.qualification.deepseek-0731-program-p-mxfp4-graph-prefill-promessi-2048-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-mxfp4-graph-prefill-promessi-2048-20261010.json
-->

# 0731 MXFP4/Q2 complete-model projection geometry including CUDA graph replay: prefill.promessi-2048

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-mxfp4-graph-prefill-promessi-2048-20261010.json)

Target identity: `ecea184b68b8c45c62e0637761fdffdd8f18663a4755e95337a2c7ebd6a1ab8a`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| projection.0.duration / prefill.promessi-2048 | 0.027136978 | s | width=2048 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.0.launches / prefill.promessi-2048 | 688 | count | width=2048 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.1.duration / prefill.promessi-2048 | 0.007332846 | s | width=4096 rows=64 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.1.launches / prefill.promessi-2048 | 336 | count | width=4096 rows=64 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.2.duration / prefill.promessi-2048 | 0.040457134 | s | width=4096 rows=256 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.2.launches / prefill.promessi-2048 | 1360 | count | width=4096 rows=256 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.3.duration / prefill.promessi-2048 | 0.318050968 | s | width=4096 rows=256 inputs=512 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.3.launches / prefill.promessi-2048 | 172 | count | width=4096 rows=256 inputs=512 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.4.duration / prefill.promessi-2048 | 0.035301768 | s | width=4096 rows=512 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.4.launches / prefill.promessi-2048 | 640 | count | width=4096 rows=512 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.5.duration / prefill.promessi-2048 | 0.010716372 | s | width=4096 rows=512 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.5.launches / prefill.promessi-2048 | 688 | count | width=4096 rows=512 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.6.duration / prefill.promessi-2048 | 0.051079914 | s | width=4096 rows=1024 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.6.launches / prefill.promessi-2048 | 672 | count | width=4096 rows=1024 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.7.duration / prefill.promessi-2048 | 0.016075516 | s | width=4096 rows=1024 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.7.launches / prefill.promessi-2048 | 688 | count | width=4096 rows=1024 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.8.duration / prefill.promessi-2048 | 0.067319702 | s | width=4096 rows=2048 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.8.launches / prefill.promessi-2048 | 1376 | count | width=4096 rows=2048 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.9.duration / prefill.promessi-2048 | 0.102730409 | s | width=4096 rows=129280 inputs=1 qtype=30 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.9.launches / prefill.promessi-2048 | 16 | count | width=4096 rows=129280 inputs=1 qtype=30 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.10.duration / prefill.promessi-2048 | 0.093201315 | s | width=8192 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.10.launches / prefill.promessi-2048 | 688 | count | width=8192 rows=4096 inputs=1 qtype=39 q8=1 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.11.duration / prefill.promessi-2048 | 0.044580008 | s | width=16384 rows=24 inputs=1 qtype=0 q8=0 block=1 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.11.launches / prefill.promessi-2048 | 1376 | count | width=16384 rows=24 inputs=1 qtype=0 q8=0 block=1 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.12.duration / prefill.promessi-2048 | 1.49852332 | s | width=16384 rows=24 inputs=512 qtype=0 q8=0 block=0 forensic=0; summed observed device duration, not additive with API waits; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| projection.12.launches / prefill.promessi-2048 | 344 | count | width=16384 rows=24 inputs=512 qtype=0 q8=0 block=0 forensic=0; includes correlated graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_up_tensorcore.duration / prefill.promessi-2048 | 3.94279787 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_up_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_mxfp4_tensorcore_wide_rows.duration / prefill.promessi-2048 | 2.36163764 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_mxfp4_tensorcore_wide_rows.launches / prefill.promessi-2048 | 600 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_reduce_native_warp.duration / prefill.promessi-2048 | 2.32482288 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_reduce_native_warp.launches / prefill.promessi-2048 | 172 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_qtype_matvec.duration / prefill.promessi-2048 | 2.31250625 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_qtype_matvec.launches / prefill.promessi-2048 | 9044 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_down_tensorcore.duration / prefill.promessi-2048 | 1.55025261 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_down_tensorcore.launches / prefill.promessi-2048 | 172 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_decoded_rows.duration / prefill.promessi-2048 | 1.44540808 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_decoded_rows.launches / prefill.promessi-2048 | 580 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_up_rows.duration / prefill.promessi-2048 | 0.895807523 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_up_rows.launches / prefill.promessi-2048 | 860 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_q8_row_matrix.duration / prefill.promessi-2048 | 0.879804062 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_q8_row_matrix.launches / prefill.promessi-2048 | 516 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_yarn_rope.duration / prefill.promessi-2048 | 0.766015538 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_yarn_rope.launches / prefill.promessi-2048 | 3416 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_candidate_scores.duration / prefill.promessi-2048 | 0.738167321 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_candidate_scores.launches / prefill.promessi-2048 | 1680 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_down_rows.duration / prefill.promessi-2048 | 0.598940804 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_moe_grouped_down_rows.launches / prefill.promessi-2048 | 860 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_rolling_rows.duration / prefill.promessi-2048 | 0.341618024 | s | Observed device population; not tensor-role identification or disjoint request stage; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| kernel.yvex_attention_rolling_rows.launches / prefill.promessi-2048 | 248 | count | Observed device kernel executions including graph replay; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/cupti.tsv |
| runtime.accelerated_matrix_launches / prefill.promessi-2048 | 1868 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.accepted_draft_tokens / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.cache_evictions / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.cache_hits / prefill.promessi-2048 | 3440 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.cache_misses / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.d2d_bytes / prefill.promessi-2048 | 4.16783653e+10 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.d2h_bytes / prefill.promessi-2048 | 455502000 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.device_allocations / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.device_synchronizations / prefill.promessi-2048 | 20 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.discarded_candidate_rows / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.downloads / prefill.promessi-2048 | 2600 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.draft_forwards / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.draft_rows / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.event_synchronizations / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.expert_bytes / prefill.promessi-2048 | 3.09591972e+11 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.expert_subviews / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.full_array_host_scan_bytes / prefill.promessi-2048 | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.generated_tokens / prefill.promessi-2048 | 16 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.graph_captures / prefill.promessi-2048 | 256 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.graph_launches / prefill.promessi-2048 | 1720 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.graph_replays / prefill.promessi-2048 | 1720 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.h2d_bytes / prefill.promessi-2048 | 698118208 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.host_allocations / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.host_payload_reads / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.kernel_launches / prefill.promessi-2048 | 53436 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.logits_d2d_bytes / prefill.promessi-2048 | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.logits_d2h_bytes / prefill.promessi-2048 | 384 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.logits_h2d_bytes / prefill.promessi-2048 | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.managed_prefetch_bytes / prefill.promessi-2048 | 0 | byte | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.mapped_bytes_touched / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.new_prefill_tokens / prefill.promessi-2048 | 2048 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.output_head_rows / prefill.promessi-2048 | 16 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.promoted_target_rows / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.prompt_tokens / prefill.promessi-2048 | 2048 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.queue_synchronizations / prefill.promessi-2048 | 2636 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.replayed_accepted_target_rows / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.reused_tokens / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.row_expert_pairs / prefill.promessi-2048 | 532512 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.target_extensions / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.target_forwards / prefill.promessi-2048 | 20 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.target_rows / prefill.promessi-2048 | 2064 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.target_verifications / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.unique_experts / prefill.promessi-2048 | 33941 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.uploads / prefill.promessi-2048 | 880 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.verified_rows / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |
| runtime.workspace_resets / prefill.promessi-2048 | 0 | count | Runtime-authored diagnostic counter; transfer bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-profile-mxfp4-graph-geometry-prefill.promessi-2048/result.json |

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
| specialization | 67ef86f1018a83dfc8118bcff8ff67bda7ab4374a253d7f25be4d00dea1b5fcf |
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
| context | 16384 |
| prefill_geometry | chunk 512; newly executed 2048 positions |
| sequence_geometry | width 1; fresh state |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature argument 1 inactive under greedy selection |
| product_path | controlled-engine native benchmark; CUPTI-instrumented; not local protocol |
| suite | deepseek-gb10-competitive-v1 / prefill.promessi-2048 |

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
