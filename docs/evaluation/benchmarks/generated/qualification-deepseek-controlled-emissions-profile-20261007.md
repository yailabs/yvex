<!-- docs:metadata
title: "Frozen batched geometry / 2K controlled native diagnostic profile"
id: yvex.evaluation.qualification.deepseek-controlled-emissions-profile-20261007
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-controlled-emissions-profile-20261007.json
-->

# Frozen batched geometry / 2K controlled native diagnostic profile

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-controlled-emissions-profile-20261007.json)

Target identity: `7253d3018f1145bfd59b38a205137999625dc43b8537c26e9a2865fd887f5184`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| request_setup.wall / request_setup | 0.809500752 | s | Server-authored phase span under CUPTI; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.kernel-union / request_setup | 0 | s | Union of clipped GPU kernel intervals; not occupancy or measured bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.kernel.count / request_setup | 0 | count | Observed clipped kernel population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.kernel.overlapping / request_setup | 0 | s | Sum of clipped kernel spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.count / request_setup | 8043 | count | Observed clipped api population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.overlapping / request_setup | 0.752793872 | s | Sum of clipped api spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.0.count / request_setup | 1273 | count | api cuMemAlloc_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.0.seconds / request_setup | 0.453797461 | s | api cuMemAlloc_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.1.count / request_setup | 1 | count | api cuMemHostAlloc grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.1.seconds / request_setup | 0.208926999 | s | api cuMemHostAlloc grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.2.count / request_setup | 1406 | count | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.2.seconds / request_setup | 0.064792437 | s | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.3.count / request_setup | 1275 | count | api cuMemGetInfo_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.api.3.seconds / request_setup | 0.014572155 | s | api cuMemGetInfo_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.copy.count / request_setup | 133 | count | Observed clipped copy population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| request_setup.copy.overlapping / request_setup | 0.000130078 | s | Sum of clipped copy spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.wall / tokenizer | 0.001298668 | s | Server-authored phase span under CUPTI; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.kernel-union / tokenizer | 0 | s | Union of clipped GPU kernel intervals; not occupancy or measured bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.kernel.count / tokenizer | 0 | count | Observed clipped kernel population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.kernel.overlapping / tokenizer | 0 | s | Sum of clipped kernel spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.api.count / tokenizer | 0 | count | Observed clipped api population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.api.overlapping / tokenizer | 0 | s | Sum of clipped api spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.copy.count / tokenizer | 0 | count | Observed clipped copy population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| tokenizer.copy.overlapping / tokenizer | 0 | s | Sum of clipped copy spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.wall / prefill | 20.2212198 | s | Server-authored phase span under CUPTI; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel-union / prefill | 18.6655598 | s | Union of clipped GPU kernel intervals; not occupancy or measured bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.count / prefill | 14196 | count | Observed clipped kernel population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.overlapping / prefill | 18.6662642 | s | Sum of clipped kernel spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.0.count / prefill | 172 | count | kernel yvex_moe_grouped_up_tensorcore grid/copy-bytes=20480 block/copy-kind=128; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.0.seconds / prefill | 3.5586715 | s | kernel yvex_moe_grouped_up_tensorcore grid/copy-bytes=20480 block/copy-kind=128; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.1.count / prefill | 172 | count | kernel yvex_attention_reduce_native_warp grid/copy-bytes=4096 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.1.seconds / prefill | 2.32001989 | s | kernel yvex_attention_reduce_native_warp grid/copy-bytes=4096 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.2.count / prefill | 172 | count | kernel yvex_moe_grouped_down_tensorcore grid/copy-bytes=40960 block/copy-kind=128; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.2.seconds / prefill | 1.61085843 | s | kernel yvex_moe_grouped_down_tensorcore grid/copy-bytes=40960 block/copy-kind=128; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.3.count / prefill | 344 | count | kernel yvex_qtype_matvec grid/copy-bytes=1536 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.3.seconds / prefill | 1.50016864 | s | kernel yvex_qtype_matvec grid/copy-bytes=1536 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.4.count / prefill | 256 | count | kernel yvex_mxfp4_tensorcore_wide_rows grid/copy-bytes=4096 block/copy-kind=128; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.4.seconds / prefill | 0.929491767 | s | kernel yvex_mxfp4_tensorcore_wide_rows grid/copy-bytes=4096 block/copy-kind=128; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.5.count / prefill | 172 | count | kernel yvex_mxfp4_tensorcore_wide_rows grid/copy-bytes=2048 block/copy-kind=128; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.5.seconds / prefill | 0.785742556 | s | kernel yvex_mxfp4_tensorcore_wide_rows grid/copy-bytes=2048 block/copy-kind=128; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.6.count / prefill | 168 | count | kernel yvex_decoded_rows grid/copy-bytes=1024 block/copy-kind=128; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.6.seconds / prefill | 0.721411138 | s | kernel yvex_decoded_rows grid/copy-bytes=1024 block/copy-kind=128; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.7.count / prefill | 428 | count | kernel yvex_attention_yarn_rope grid/copy-bytes=4096 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.kernel.7.seconds / prefill | 0.710410569 | s | kernel yvex_attention_yarn_rope grid/copy-bytes=4096 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.count / prefill | 54472 | count | Observed clipped api population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.overlapping / prefill | 19.3501783 | s | Sum of clipped api spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.0.count / prefill | 1282 | count | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.0.seconds / prefill | 11.153111 | s | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.1.count / prefill | 524 | count | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.1.seconds / prefill | 6.88805023 | s | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.2.count / prefill | 516 | count | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.2.seconds / prefill | 1.00676483 | s | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.3.count / prefill | 517 | count | api cuMemAlloc_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.api.3.seconds / prefill | 0.076924939 | s | api cuMemAlloc_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.copy.count / prefill | 17510 | count | Observed clipped copy population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| prefill.copy.overlapping / prefill | 0.343169825 | s | Sum of clipped copy spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.wall / first_decode | 0.14941092 | s | Server-authored phase span under CUPTI; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel-union / first_decode | 0.098149519 | s | Union of clipped GPU kernel intervals; not occupancy or measured bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.count / first_decode | 2421 | count | Observed clipped kernel population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.overlapping / first_decode | 0.098170095 | s | Sum of clipped kernel spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.0.count / first_decode | 43 | count | kernel yvex_attention_reduce_native grid/copy-bytes=64 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.0.seconds / first_decode | 0.015358176 | s | kernel yvex_attention_reduce_native grid/copy-bytes=64 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.1.count / first_decode | 86 | count | kernel yvex_residual_mhc_pre grid/copy-bytes=1 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.1.seconds / first_decode | 0.009035007 | s | kernel yvex_residual_mhc_pre grid/copy-bytes=1 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.2.count / first_decode | 43 | count | kernel yvex_moe_grouped_up_rows grid/copy-bytes=1536 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.2.seconds / first_decode | 0.008392325 | s | kernel yvex_moe_grouped_up_rows grid/copy-bytes=1536 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.3.count / first_decode | 43 | count | kernel yvex_decoded_mxfp4_rows grid/copy-bytes=1024 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.3.seconds / first_decode | 0.008303011 | s | kernel yvex_decoded_mxfp4_rows grid/copy-bytes=1024 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.4.count / first_decode | 86 | count | kernel yvex_qtype_matvec grid/copy-bytes=512 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.4.seconds / first_decode | 0.007481461 | s | kernel yvex_qtype_matvec grid/copy-bytes=512 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.5.count / first_decode | 1 | count | kernel yvex_qtype_matvec grid/copy-bytes=16160 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.5.seconds / first_decode | 0.006365474 | s | kernel yvex_qtype_matvec grid/copy-bytes=16160 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.6.count / first_decode | 43 | count | kernel yvex_moe_grouped_down_rows grid/copy-bytes=3072 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.6.seconds / first_decode | 0.006318943 | s | kernel yvex_moe_grouped_down_rows grid/copy-bytes=3072 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.7.count / first_decode | 43 | count | kernel yvex_mxfp4_q8_rows grid/copy-bytes=4096 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.kernel.7.seconds / first_decode | 0.005673774 | s | kernel yvex_mxfp4_q8_rows grid/copy-bytes=4096 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.count / first_decode | 10617 | count | Observed clipped api population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.overlapping / first_decode | 0.123216654 | s | Sum of clipped api spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.0.count / first_decode | 216 | count | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.0.seconds / first_decode | 0.068066489 | s | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.1.count / first_decode | 133 | count | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.1.seconds / first_decode | 0.024004611 | s | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.2.count / first_decode | 86 | count | api cuGraphInstantiateWithFlags grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.2.seconds / first_decode | 0.00890422 | s | api cuGraphInstantiateWithFlags grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.3.count / first_decode | 129 | count | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.api.3.seconds / first_decode | 0.006498407 | s | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.copy.count / first_decode | 2193 | count | Observed clipped copy population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| first_decode.copy.overlapping / first_decode | 0.004018806 | s | Sum of clipped copy spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.wall / post_first_decode | 2.04575028 | s | Server-authored phase span under CUPTI; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel-union / post_first_decode | 1.4981839 | s | Union of clipped GPU kernel intervals; not occupancy or measured bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.count / post_first_decode | 36819 | count | Observed clipped kernel population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.overlapping / post_first_decode | 1.4982407 | s | Sum of clipped kernel spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.0.count / post_first_decode | 645 | count | kernel yvex_attention_reduce_native grid/copy-bytes=64 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.0.seconds / post_first_decode | 0.244368387 | s | kernel yvex_attention_reduce_native grid/copy-bytes=64 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.1.count / post_first_decode | 1290 | count | kernel yvex_residual_mhc_pre grid/copy-bytes=1 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.1.seconds / post_first_decode | 0.135088072 | s | kernel yvex_residual_mhc_pre grid/copy-bytes=1 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.2.count / post_first_decode | 645 | count | kernel yvex_moe_grouped_up_rows grid/copy-bytes=1536 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.2.seconds / post_first_decode | 0.127835827 | s | kernel yvex_moe_grouped_up_rows grid/copy-bytes=1536 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.3.count / post_first_decode | 645 | count | kernel yvex_decoded_mxfp4_rows grid/copy-bytes=1024 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.3.seconds / post_first_decode | 0.127209801 | s | kernel yvex_decoded_mxfp4_rows grid/copy-bytes=1024 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.4.count / post_first_decode | 1290 | count | kernel yvex_qtype_matvec grid/copy-bytes=512 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.4.seconds / post_first_decode | 0.112750698 | s | kernel yvex_qtype_matvec grid/copy-bytes=512 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.5.count / post_first_decode | 645 | count | kernel yvex_moe_grouped_down_rows grid/copy-bytes=3072 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.5.seconds / post_first_decode | 0.095975675 | s | kernel yvex_moe_grouped_down_rows grid/copy-bytes=3072 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.6.count / post_first_decode | 15 | count | kernel yvex_qtype_matvec grid/copy-bytes=16160 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.6.seconds / post_first_decode | 0.095454758 | s | kernel yvex_qtype_matvec grid/copy-bytes=16160 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.7.count / post_first_decode | 645 | count | kernel yvex_mxfp4_q8_rows grid/copy-bytes=4096 block/copy-kind=256; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.kernel.7.seconds / post_first_decode | 0.085219826 | s | kernel yvex_mxfp4_q8_rows grid/copy-bytes=4096 block/copy-kind=256; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.count / post_first_decode | 117745 | count | Observed clipped api population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.overlapping / post_first_decode | 1.7308285 | s | Sum of clipped api spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.0.count / post_first_decode | 3240 | count | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.0.seconds / post_first_decode | 1.04469365 | s | api cuCtxSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.1.count / post_first_decode | 1995 | count | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.1.seconds / post_first_decode | 0.369565287 | s | api cuStreamSynchronize grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.2.count / post_first_decode | 1935 | count | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.2.seconds / post_first_decode | 0.098347317 | s | api cuMemFree_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.3.count / post_first_decode | 22923 | count | api cuMemcpyDtoDAsync_v2 grid/copy-bytes=0 block/copy-kind=0; actual observed population; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.api.3.seconds / post_first_decode | 0.071629991 | s | api cuMemcpyDtoDAsync_v2 grid/copy-bytes=0 block/copy-kind=0; profiled clipped duration, not unprofiled throughput; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.copy.count / post_first_decode | 31623 | count | Observed clipped copy population in this phase; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |
| post_first_decode.copy.overlapping / post_first_decode | 0.056097934 | s | Sum of clipped copy spans; not additive with other owners; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/followup-20261007.iiQ6tR/yvex-emission-batch-profile-01/profile-summary.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Source-frozen kernel/API activity; no numerical, quality or performance qualification;  |
| checkpoint-reference | BLOCKED | Source-frozen kernel/API activity; no numerical, quality or performance qualification; No admitted independent checkpoint-matched full-model numerical/quality comparison in this timing series |
| representation-quality | BLOCKED | Source-frozen kernel/API activity; no numerical, quality or performance qualification; No admitted independent checkpoint-matched full-model numerical/quality comparison in this timing series |
| backend-execution | CHARACTERIZED | Source-frozen kernel/API activity; no numerical, quality or performance qualification;  |
| deployment-performance | UNQUALIFIED | Source-frozen kernel/API activity; no numerical, quality or performance qualification;  |
| product-path | UNQUALIFIED | Source-frozen kernel/API activity; no numerical, quality or performance qualification;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | 4d489a7e6340ca30f5cd6e72af004347eb246e2b9521eaf6f7c6ae56b5f995dd |
| transformation_ir | f1fca7b4ec04d1b0de2a0f0707b3f78c5600e9a6486a83c6fc9f3a4bd70f88e8 |
| physical_policy | 59dd7bdabf6b81989dfa14e0f70692805a8f02a473afcc040a3e55083f48dda0 |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | 36c16dfb1a944d359fa7136cee0497cc7e2f3d6e |
| source_tree | 82b6643866c9882819107aee2354139ee1046b49 |
| source_delta | 6ec17a3c151733a135b2f46d063e4a49b65948e288fc3035ddceed48fa18e05b |
| build | 28dfaa292efd6891a9b06541109ef7633f0909d216d2b6f61666a570939f9159 |
| executable | 87cfaee9a0ead3fc44b7540f7029425c2feabe48b821e9c814ab4736ece7ce61 |
| backend | cuda |
| backend_implementation | backend.cuda@36c16dfb1a944d359fa7136cee0497cc7e2f3d6e+6ec17a3c151733a135b2f46d063e4a49b65948e288fc3035ddceed48fa18e05b |
| kernel_bundle | c6564e2eeb74331722c155f18e00d736d6f9b61117f7a8cb132700819d7addc8 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | d17961844d6abc9d3cc7a7d3b776458cb0119920025e712e0ca3019b01fb9c33 |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | native stochastic=0, temperature=0; no stochastic draws |
| product_path | controlled-engine/public-C-host/native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Diagnostic profiled timing, excluded from performance series.
- API waits and device work overlap; sums are not additive removable overhead.
- Kernel union is activity, not occupancy, Tensor Core utilization or measured bandwidth.
- Boundary CPU physical mapping snapshots are not GPU page-fault counters.
- One diagnostic non-thinking controlled public-C/native-v25 turn; not the installed Rust/chat product.
- No profiled duration enters a performance series; measurements is intentionally empty.
- Exact source and kernel relationship comes from the separately unprofiled receipt; no claim is inherited.
- Checkpoint reference and representation quality remain BLOCKED; no 20/700 performance gate is earned.
