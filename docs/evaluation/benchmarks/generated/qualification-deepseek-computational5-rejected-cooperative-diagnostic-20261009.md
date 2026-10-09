<!-- docs:metadata
title: "DIAGNOSTIC device/API activity / REJECTED execution candidate / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none"
id: yvex.evaluation.qualification.deepseek-computational5-rejected-cooperative-diagnostic-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-computational5-rejected-cooperative-diagnostic-20261009.json
-->

# DIAGNOSTIC device/API activity / REJECTED execution candidate / GB10 controlled configuration / Rust-native prefill.promessi-2048 / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-computational5-rejected-cooperative-diagnostic-20261009.json)

Target identity: `32723b394339f1ea5d98950010faeef638671d3794d4f1df10bab98facdcea81`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.prompt_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.reused_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.prefill_tokens / prefill.promessi-2048 | 2048 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.generated_tokens / prefill.promessi-2048 | 16 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.proposed_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.selected_verification_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.accepted_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.rejected_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| turn.discarded_tokens / prefill.promessi-2048 | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/native/receipt.json |
| request_setup.wall_seconds / prefill.promessi-2048/request_setup | 0.545620539 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.observed_device_seconds / prefill.promessi-2048/request_setup | 0.039362916 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api_without_observed_device_seconds / prefill.promessi-2048/request_setup | 0.491542615 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.outside_both_seconds / prefill.promessi-2048/request_setup | 0.014715008 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.count / prefill.promessi-2048/request_setup | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.union_seconds / prefill.promessi-2048/request_setup | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.kernel.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.count / prefill.promessi-2048/request_setup | 133 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.union_seconds / prefill.promessi-2048/request_setup | 9.8276e-05 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.copy.logical_bytes / prefill.promessi-2048/request_setup | 987704 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.count / prefill.promessi-2048/request_setup | 499 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.union_seconds / prefill.promessi-2048/request_setup | 0.03926464 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.memset.logical_bytes / prefill.promessi-2048/request_setup | 7.67371476e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.count / prefill.promessi-2048/request_setup | 3399 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.union_seconds / prefill.promessi-2048/request_setup | 0.530904071 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| request_setup.api.logical_bytes / prefill.promessi-2048/request_setup | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.wall_seconds / prefill.promessi-2048/tokenizer | 0.001028355 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api_without_observed_device_seconds / prefill.promessi-2048/tokenizer | 0 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.outside_both_seconds / prefill.promessi-2048/tokenizer | 0.001028355 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.count / prefill.promessi-2048/tokenizer | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.kernel.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.count / prefill.promessi-2048/tokenizer | 0 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.copy.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.count / prefill.promessi-2048/tokenizer | 0 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.memset.logical_bytes / prefill.promessi-2048/tokenizer | 0 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.count / prefill.promessi-2048/tokenizer | 0 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.union_seconds / prefill.promessi-2048/tokenizer | 0 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| tokenizer.api.logical_bytes / prefill.promessi-2048/tokenizer | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.wall_seconds / prefill.promessi-2048/prefill | 20.7401431 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.observed_device_seconds / prefill.promessi-2048/prefill | 20.3187782 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.api_without_observed_device_seconds / prefill.promessi-2048/prefill | 0.106369765 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.outside_both_seconds / prefill.promessi-2048/prefill | 0.314995078 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.count / prefill.promessi-2048/prefill | 14196 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.union_seconds / prefill.promessi-2048/prefill | 19.7859259 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.kernel.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.count / prefill.promessi-2048/prefill | 17510 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.union_seconds / prefill.promessi-2048/prefill | 0.336009202 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.copy.logical_bytes / prefill.promessi-2048/prefill | 4.09891304e+10 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.count / prefill.promessi-2048/prefill | 5882 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.union_seconds / prefill.promessi-2048/prefill | 0.196843142 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.memset.logical_bytes / prefill.promessi-2048/prefill | 3.82768529e+10 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.count / prefill.promessi-2048/prefill | 52582 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.union_seconds / prefill.promessi-2048/prefill | 20.3489281 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| prefill.api.logical_bytes / prefill.promessi-2048/prefill | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.wall_seconds / prefill.promessi-2048/first_decode | 0.131957553 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.observed_device_seconds / prefill.promessi-2048/first_decode | 0.101171964 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api_without_observed_device_seconds / prefill.promessi-2048/first_decode | 0.020765693 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.outside_both_seconds / prefill.promessi-2048/first_decode | 0.010019896 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.count / prefill.promessi-2048/first_decode | 2421 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.union_seconds / prefill.promessi-2048/first_decode | 0.096109439 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.kernel.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.count / prefill.promessi-2048/first_decode | 2193 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.union_seconds / prefill.promessi-2048/first_decode | 0.004147783 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.copy.logical_bytes / prefill.promessi-2048/first_decode | 140196864 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.count / prefill.promessi-2048/first_decode | 1433 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.union_seconds / prefill.promessi-2048/first_decode | 0.000914742 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.memset.logical_bytes / prefill.promessi-2048/first_decode | 32875228 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.count / prefill.promessi-2048/first_decode | 10144 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.union_seconds / prefill.promessi-2048/first_decode | 0.120115113 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| first_decode.api.logical_bytes / prefill.promessi-2048/first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.wall_seconds / prefill.promessi-2048/post_first_decode | 1.79171763 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.observed_device_seconds / prefill.promessi-2048/post_first_decode | 1.49855898 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api_without_observed_device_seconds / prefill.promessi-2048/post_first_decode | 0.167233003 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.outside_both_seconds / prefill.promessi-2048/post_first_decode | 0.125925649 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.count / prefill.promessi-2048/post_first_decode | 36819 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.union_seconds / prefill.promessi-2048/post_first_decode | 1.43536513 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.kernel.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.count / prefill.promessi-2048/post_first_decode | 31623 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.union_seconds / prefill.promessi-2048/post_first_decode | 0.052407033 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.copy.logical_bytes / prefill.promessi-2048/post_first_decode | 1.99873118e+09 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.count / prefill.promessi-2048/post_first_decode | 21495 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.union_seconds / prefill.promessi-2048/post_first_decode | 0.010786821 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.memset.logical_bytes / prefill.promessi-2048/post_first_decode | 493344804 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.count / prefill.promessi-2048/post_first_decode | 110651 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.union_seconds / prefill.promessi-2048/post_first_decode | 1.6424131 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |
| post_first_decode.api.logical_bytes / prefill.promessi-2048/post_first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-computational-20261009.v9tUlTUe/cooperative-expert-prefill2k-profile-01/device-activity-summary.json |

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
| source_delta | 4e0189432ac0f89fba4d37e0aee5497b7725ce6776150e30744c8a1711479f1b |
| build | 20f3391399e778a2d08be31e6181bf119c344e432d6efa1cea6187e6b88e9156 |
| executable | 9f648d8ca80c6b3df99a5955f124903c39f41dcff6e8af36d1e836a24cf4d33e |
| backend | cuda |
| backend_implementation | backend.cuda@25b981808348440f37cde5913d4d804c4940e5a7+4e0189432ac0f89fba4d37e0aee5497b7725ce6776150e30744c8a1711479f1b |
| kernel_bundle | 59086ad54cc6225109474f42f1351b3a91a8cc6fcb6983ee63349f308b9dc638 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663165952 |
| runtime_configuration | fc4957d2f1947fc95dfac9dec430ec8b4a1684d5888cabfa662b7b95250821f6 |
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
