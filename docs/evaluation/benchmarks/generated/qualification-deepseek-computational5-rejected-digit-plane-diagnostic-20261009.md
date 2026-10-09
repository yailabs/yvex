<!-- docs:metadata
title: "DIAGNOSTIC device/API activity / REJECTED execution candidate / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none"
id: yvex.evaluation.qualification.deepseek-computational5-rejected-digit-plane-diagnostic-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-rejected-digit-plane-diagnostic-20261009.json
-->

# DIAGNOSTIC device/API activity / REJECTED execution candidate / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-rejected-digit-plane-diagnostic-20261009.json)

Target identity: `77119ded31eb260c5f8489efe5f17235f6b481ad5669469ad0677acfe8df6b37`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.prompt_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.reused_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.prefill_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.generated_tokens / prefill.promessi-2048 | 16 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.proposed_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.selected_verification_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.accepted_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.rejected_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| turn.discarded_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/native/receipt.json |
| request_setup.wall_seconds / prefill.promessi-2048/request_setup | 2.66946387 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.observed_device_seconds / prefill.promessi-2048/request_setup | 0.059355664 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api_without_observed_device_seconds / prefill.promessi-2048/request_setup | 2.59510586 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.outside_both_seconds / prefill.promessi-2048/request_setup | 0.015002349 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.count / prefill.promessi-2048/request_setup | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.union_seconds / prefill.promessi-2048/request_setup | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.count / prefill.promessi-2048/request_setup | 133 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.union_seconds / prefill.promessi-2048/request_setup | 9.9904e-05 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.logical_bytes / prefill.promessi-2048/request_setup | 987704 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.count / prefill.promessi-2048/request_setup | 499 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.union_seconds / prefill.promessi-2048/request_setup | 0.05925576 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.logical_bytes / prefill.promessi-2048/request_setup | 1.15558431e+10 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.count / prefill.promessi-2048/request_setup | 3399 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.union_seconds / prefill.promessi-2048/request_setup | 2.65446152 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.wall_seconds / prefill.promessi-2048/tokenizer | 0.001011646 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api_without_observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.outside_both_seconds / prefill.promessi-2048/tokenizer | 0.001011646 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.count / prefill.promessi-2048/tokenizer | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.count / prefill.promessi-2048/tokenizer | 0 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.count / prefill.promessi-2048/tokenizer | 0 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.count / prefill.promessi-2048/tokenizer | 0 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.wall_seconds / prefill.promessi-2048/prefill | 27.5675173 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.observed_device_seconds / prefill.promessi-2048/prefill | 27.1217423 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.api_without_observed_device_seconds / prefill.promessi-2048/prefill | 0.123922911 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.outside_both_seconds / prefill.promessi-2048/prefill | 0.321852046 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.count / prefill.promessi-2048/prefill | 14540 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.union_seconds / prefill.promessi-2048/prefill | 26.585593 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.count / prefill.promessi-2048/prefill | 17510 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.union_seconds / prefill.promessi-2048/prefill | 0.3383771 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.logical_bytes / prefill.promessi-2048/prefill | 4.09891304e+10 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.count / prefill.promessi-2048/prefill | 5882 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.union_seconds / prefill.promessi-2048/prefill | 0.197772195 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.logical_bytes / prefill.promessi-2048/prefill | 3.82768529e+10 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.count / prefill.promessi-2048/prefill | 53011 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.union_seconds / prefill.promessi-2048/prefill | 27.1665539 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.wall_seconds / prefill.promessi-2048/first_decode | 0.132690877 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.observed_device_seconds / prefill.promessi-2048/first_decode | 0.10145067 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api_without_observed_device_seconds / prefill.promessi-2048/first_decode | 0.020631451 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.outside_both_seconds / prefill.promessi-2048/first_decode | 0.010608756 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.count / prefill.promessi-2048/first_decode | 2421 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.union_seconds / prefill.promessi-2048/first_decode | 0.096569336 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.count / prefill.promessi-2048/first_decode | 2193 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.union_seconds / prefill.promessi-2048/first_decode | 0.004095096 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.logical_bytes / prefill.promessi-2048/first_decode | 140196864 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.count / prefill.promessi-2048/first_decode | 1433 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.union_seconds / prefill.promessi-2048/first_decode | 0.000786238 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.logical_bytes / prefill.promessi-2048/first_decode | 32875228 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.count / prefill.promessi-2048/first_decode | 10143 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.union_seconds / prefill.promessi-2048/first_decode | 0.120266035 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.wall_seconds / prefill.promessi-2048/post_first_decode | 1.8106732 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.observed_device_seconds / prefill.promessi-2048/post_first_decode | 1.50215901 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api_without_observed_device_seconds / prefill.promessi-2048/post_first_decode | 0.173778804 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.outside_both_seconds / prefill.promessi-2048/post_first_decode | 0.134735387 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.count / prefill.promessi-2048/post_first_decode | 36819 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.union_seconds / prefill.promessi-2048/post_first_decode | 1.43936118 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.count / prefill.promessi-2048/post_first_decode | 31623 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.union_seconds / prefill.promessi-2048/post_first_decode | 0.052183355 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.logical_bytes / prefill.promessi-2048/post_first_decode | 1.99873118e+09 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.count / prefill.promessi-2048/post_first_decode | 21495 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.union_seconds / prefill.promessi-2048/post_first_decode | 0.010614475 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.logical_bytes / prefill.promessi-2048/post_first_decode | 493344804 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.count / prefill.promessi-2048/post_first_decode | 110652 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.union_seconds / prefill.promessi-2048/post_first_decode | 1.65230583 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/digit-plane-prefill2k-profile-01/device-activity-summary.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Not independently qualified by this timing capture;  |
| checkpoint-reference | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |
| deployment-performance | UNQUALIFIED | One instrumented diagnostic sample, not a timed performance series;  |
| family-conformance | UNQUALIFIED | Not independently qualified by this timing capture;  |
| product-path | CHARACTERIZED | One profiled native turn; observed execution only, not throughput;  |
| representation-quality | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |

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
| source_commit | 25b981808348440f37cde5913d4d804c4940e5a7 |
| source_tree | a6510628c9b45227f5bd4624a1f7acd0b9c4674c |
| source_delta | 6d7175fa8395548acb4e52c1addfafc11400f829207de693dd743075918b81f0 |
| build | 517c9ffb220e145022296be637046c7dbb5aa0efb48e10be227f79ca30448994 |
| executable | 7776b66b7a2a3cd4540ef6c58adaed3f83a99781805b88001faff73e833aac79 |
| backend | cuda |
| backend_implementation | backend.cuda@25b981808348440f37cde5913d4d804c4940e5a7+6d7175fa8395548acb4e52c1addfafc11400f829207de693dd743075918b81f0 |
| kernel_bundle | 1f0b883e115a10e2b810b5d8c6f10915f83486f75f029b2631525fa4bfde86b3 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663165952 |
| runtime_configuration | 58d40186f859ebab02cdec6d42c145d0b6989ede0cba0611b3569c136bdbfd48 |
| context | 16384 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-configuration/native-v25 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One profiled diagnostic sample, not an unprofiled performance result.
- GPU activity includes kernels, copies and memset; not all engines or hardware utilization.
- API time overlaps device activity. Missing activity is not automatically removable CPU cost.
- Bytes are logical API traffic, not measured physical DRAM bandwidth.
- Union by kind may overlap other kinds; only the partition is disjoint.
- Exact native configuration is retained. Profiling records cannot enter performance comparison.
- No independent model/representation-quality claim follows from device/API timing.
- Hardware-counter utilization, occupancy and physical bandwidth were not measured.
- Host first-fragment publication time is unavailable; server token and client visibility clocks remain separate.
