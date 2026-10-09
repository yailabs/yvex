<!-- docs:metadata
title: "DIAGNOSTIC device/API activity / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none"
id: yvex.evaluation.qualification.deepseek-computational5-baseline-prefill-diagnostic-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-baseline-prefill-diagnostic-20261009.json
-->

# DIAGNOSTIC device/API activity / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-baseline-prefill-diagnostic-20261009.json)

Target identity: `c203dc75c22c744cae01c7fce0d54471309cc9398b1a5ddf49f061eea21c459e`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.prompt_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.reused_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.prefill_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.generated_tokens / prefill.promessi-2048 | 16 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.proposed_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.selected_verification_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.accepted_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.rejected_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| turn.discarded_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/native/receipt.json |
| request_setup.wall_seconds / prefill.promessi-2048/request_setup | 0.555859175 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.observed_device_seconds / prefill.promessi-2048/request_setup | 0.039496228 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api_without_observed_device_seconds / prefill.promessi-2048/request_setup | 0.498395897 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.outside_both_seconds / prefill.promessi-2048/request_setup | 0.01796705 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.count / prefill.promessi-2048/request_setup | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.union_seconds / prefill.promessi-2048/request_setup | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.count / prefill.promessi-2048/request_setup | 133 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.union_seconds / prefill.promessi-2048/request_setup | 9.7568e-05 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.logical_bytes / prefill.promessi-2048/request_setup | 987704 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.count / prefill.promessi-2048/request_setup | 499 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.union_seconds / prefill.promessi-2048/request_setup | 0.03939866 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.logical_bytes / prefill.promessi-2048/request_setup | 7.67371476e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.count / prefill.promessi-2048/request_setup | 3399 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.union_seconds / prefill.promessi-2048/request_setup | 0.537889914 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.wall_seconds / prefill.promessi-2048/tokenizer | 0.001069998 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api_without_observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.outside_both_seconds / prefill.promessi-2048/tokenizer | 0.001069998 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.count / prefill.promessi-2048/tokenizer | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.count / prefill.promessi-2048/tokenizer | 0 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.count / prefill.promessi-2048/tokenizer | 0 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.count / prefill.promessi-2048/tokenizer | 0 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.wall_seconds / prefill.promessi-2048/prefill | 19.5824508 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.observed_device_seconds / prefill.promessi-2048/prefill | 19.1682813 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.api_without_observed_device_seconds / prefill.promessi-2048/prefill | 0.089766377 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.outside_both_seconds / prefill.promessi-2048/prefill | 0.324403122 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.count / prefill.promessi-2048/prefill | 14196 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.union_seconds / prefill.promessi-2048/prefill | 18.6355494 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.count / prefill.promessi-2048/prefill | 17510 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.union_seconds / prefill.promessi-2048/prefill | 0.334483923 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.logical_bytes / prefill.promessi-2048/prefill | 4.09891304e+10 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.count / prefill.promessi-2048/prefill | 5882 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.union_seconds / prefill.promessi-2048/prefill | 0.198248041 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.logical_bytes / prefill.promessi-2048/prefill | 3.82768529e+10 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.count / prefill.promessi-2048/prefill | 52581 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.union_seconds / prefill.promessi-2048/prefill | 19.1802918 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.wall_seconds / prefill.promessi-2048/first_decode | 0.140367448 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.observed_device_seconds / prefill.promessi-2048/first_decode | 0.106977375 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api_without_observed_device_seconds / prefill.promessi-2048/first_decode | 0.022836453 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.outside_both_seconds / prefill.promessi-2048/first_decode | 0.01055362 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.count / prefill.promessi-2048/first_decode | 2421 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.union_seconds / prefill.promessi-2048/first_decode | 0.101287112 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.count / prefill.promessi-2048/first_decode | 2193 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.union_seconds / prefill.promessi-2048/first_decode | 0.004593335 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.logical_bytes / prefill.promessi-2048/first_decode | 140196864 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.count / prefill.promessi-2048/first_decode | 1433 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.union_seconds / prefill.promessi-2048/first_decode | 0.001096928 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.logical_bytes / prefill.promessi-2048/first_decode | 32875228 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.count / prefill.promessi-2048/first_decode | 10144 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.union_seconds / prefill.promessi-2048/first_decode | 0.127724765 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.wall_seconds / prefill.promessi-2048/post_first_decode | 1.78667378 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.observed_device_seconds / prefill.promessi-2048/post_first_decode | 1.48292596 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api_without_observed_device_seconds / prefill.promessi-2048/post_first_decode | 0.170142112 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.outside_both_seconds / prefill.promessi-2048/post_first_decode | 0.133605709 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.count / prefill.promessi-2048/post_first_decode | 36819 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.union_seconds / prefill.promessi-2048/post_first_decode | 1.42160191 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.count / prefill.promessi-2048/post_first_decode | 31623 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.union_seconds / prefill.promessi-2048/post_first_decode | 0.051140357 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.logical_bytes / prefill.promessi-2048/post_first_decode | 1.99873118e+09 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.count / prefill.promessi-2048/post_first_decode | 21495 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.union_seconds / prefill.promessi-2048/post_first_decode | 0.010183693 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.logical_bytes / prefill.promessi-2048/post_first_decode | 493344804 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.count / prefill.promessi-2048/post_first_decode | 110650 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.union_seconds / prefill.promessi-2048/post_first_decode | 1.62932036 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/baseline-target-prefill2k-profile-01/device-activity-summary.json |

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
| runtime_configuration | 1faf77f32c0ab6d2a58e1348d4df6e969e95319b112b183bc32559bdef534243 |
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
