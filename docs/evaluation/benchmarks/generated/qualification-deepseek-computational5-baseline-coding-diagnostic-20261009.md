<!-- docs:metadata
title: "DIAGNOSTIC device/API activity / GB10 controlled configuration / Rust-native coding.hash-table / none"
id: yvex.evaluation.qualification.deepseek-computational5-baseline-coding-diagnostic-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-baseline-coding-diagnostic-20261009.json
-->

# DIAGNOSTIC device/API activity / GB10 controlled configuration / Rust-native coding.hash-table / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-baseline-coding-diagnostic-20261009.json)

Target identity: `454d6ee03a424b65b3a1dfc3ee843dd5875fbd66c6045e8eae734588c919aed5`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.prompt_tokens / coding.hash-table | 31 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.reused_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.prefill_tokens / coding.hash-table | 31 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.generated_tokens / coding.hash-table | 256 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.proposed_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.selected_verification_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.accepted_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.rejected_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| turn.discarded_tokens / coding.hash-table | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/native/receipt.json |
| request_setup.wall_seconds / coding.hash-table/request_setup | 0.461264587 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.observed_device_seconds / coding.hash-table/request_setup | 0.03927703 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.api_without_observed_device_seconds / coding.hash-table/request_setup | 0.407188897 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.outside_both_seconds / coding.hash-table/request_setup | 0.01479866 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.count / coding.hash-table/request_setup | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.union_seconds / coding.hash-table/request_setup | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.logical_bytes / coding.hash-table/request_setup | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.copy.count / coding.hash-table/request_setup | 133 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.copy.union_seconds / coding.hash-table/request_setup | 9.8944e-05 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.copy.logical_bytes / coding.hash-table/request_setup | 987704 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.memset.count / coding.hash-table/request_setup | 499 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.memset.union_seconds / coding.hash-table/request_setup | 0.039178086 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.memset.logical_bytes / coding.hash-table/request_setup | 7.663442e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.api.count / coding.hash-table/request_setup | 3399 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.api.union_seconds / coding.hash-table/request_setup | 0.446464703 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| request_setup.api.logical_bytes / coding.hash-table/request_setup | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.wall_seconds / coding.hash-table/tokenizer | 0.000378847 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.observed_device_seconds / coding.hash-table/tokenizer | 0 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.api_without_observed_device_seconds / coding.hash-table/tokenizer | 0 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.outside_both_seconds / coding.hash-table/tokenizer | 0.000378847 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.count / coding.hash-table/tokenizer | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.union_seconds / coding.hash-table/tokenizer | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.logical_bytes / coding.hash-table/tokenizer | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.count / coding.hash-table/tokenizer | 0 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.union_seconds / coding.hash-table/tokenizer | 0 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.logical_bytes / coding.hash-table/tokenizer | 0 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.count / coding.hash-table/tokenizer | 0 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.union_seconds / coding.hash-table/tokenizer | 0 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.logical_bytes / coding.hash-table/tokenizer | 0 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.api.count / coding.hash-table/tokenizer | 0 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.api.union_seconds / coding.hash-table/tokenizer | 0 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| tokenizer.api.logical_bytes / coding.hash-table/tokenizer | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.wall_seconds / coding.hash-table/prefill | 0.767805256 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.observed_device_seconds / coding.hash-table/prefill | 0.652586987 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.api_without_observed_device_seconds / coding.hash-table/prefill | 0.035923104 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.outside_both_seconds / coding.hash-table/prefill | 0.079295165 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.kernel.count / coding.hash-table/prefill | 2569 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.kernel.union_seconds / coding.hash-table/prefill | 0.640772253 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.kernel.logical_bytes / coding.hash-table/prefill | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.copy.count / coding.hash-table/prefill | 2309 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.copy.union_seconds / coding.hash-table/prefill | 0.006560756 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.copy.logical_bytes / coding.hash-table/prefill | 687481200 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.memset.count / coding.hash-table/prefill | 1533 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.memset.union_seconds / coding.hash-table/prefill | 0.005253978 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.memset.logical_bytes / coding.hash-table/prefill | 817854268 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.api.count / coding.hash-table/prefill | 11899 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.api.union_seconds / coding.hash-table/prefill | 0.685223293 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| prefill.api.logical_bytes / coding.hash-table/prefill | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.wall_seconds / coding.hash-table/first_decode | 0.125226817 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.observed_device_seconds / coding.hash-table/first_decode | 0.088898915 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.api_without_observed_device_seconds / coding.hash-table/first_decode | 0.025533417 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.outside_both_seconds / coding.hash-table/first_decode | 0.010794485 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.count / coding.hash-table/first_decode | 2547 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.union_seconds / coding.hash-table/first_decode | 0.084255469 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.logical_bytes / coding.hash-table/first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.copy.count / coding.hash-table/first_decode | 2240 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.copy.union_seconds / coding.hash-table/first_decode | 0.003652505 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.copy.logical_bytes / coding.hash-table/first_decode | 87606736 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.memset.count / coding.hash-table/first_decode | 1456 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.memset.union_seconds / coding.hash-table/first_decode | 0.000990941 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.memset.logical_bytes / coding.hash-table/first_decode | 79615532 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.api.count / coding.hash-table/first_decode | 10673 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.api.union_seconds / coding.hash-table/first_decode | 0.112628123 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| first_decode.api.logical_bytes / coding.hash-table/first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.wall_seconds / coding.hash-table/post_first_decode | 27.3570809 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.observed_device_seconds / coding.hash-table/post_first_decode | 22.0734821 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.api_without_observed_device_seconds / coding.hash-table/post_first_decode | 2.9288726 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.outside_both_seconds / coding.hash-table/post_first_decode | 2.35472626 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.count / coding.hash-table/post_first_decode | 625413 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.union_seconds / coding.hash-table/post_first_decode | 21.0869415 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.logical_bytes / coding.hash-table/post_first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.count / coding.hash-table/post_first_decode | 538794 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.union_seconds / coding.hash-table/post_first_decode | 0.805576673 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.logical_bytes / coding.hash-table/post_first_decode | 2.55444225e+10 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.count / coding.hash-table/post_first_decode | 365415 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.union_seconds / coding.hash-table/post_first_decode | 0.180963844 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.logical_bytes / coding.hash-table/post_first_decode | 7.99185999e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.count / coding.hash-table/post_first_decode | 1866776 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.union_seconds / coding.hash-table/post_first_decode | 24.6121033 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.logical_bytes / coding.hash-table/post_first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-coding-profile-01/device-activity-summary.json |

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
| source_commit | 337e7e73057abd8a784a67eea58ad8a6f079869d |
| source_tree | 14b7fea18275a542739d4d2afaa38f6fd720d407 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | a72b5d98f4361eb97eb051d761a7133268d1f97da66d7543d9fecc7046a0b857 |
| executable | eb108d22b68674f92a5430e246723e4e7f0505ae4d0eef01beb590845a8fba7d |
| backend | cuda |
| backend_implementation | backend.cuda@337e7e73057abd8a784a67eea58ad8a6f079869d+e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| kernel_bundle | c4157a8b2964602100bcb1bb4f895b044ad8ca221b4098b778c67d176dbf2468 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663165952 |
| runtime_configuration | ab1fc9a6c4f55b289812b00c0757354d26684f40a3fa1fa9c0131597f8102a06 |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-configuration/native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

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
