<!-- docs:metadata
title: "DIAGNOSTIC device/API activity / GB10 Rust/native product coding.metal / none"
id: yvex.evaluation.qualification.deepseek-native-device-api-diagnostic-coding-none-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-device-api-diagnostic-coding-none-20261008.json
-->

# DIAGNOSTIC device/API activity / GB10 Rust/native product coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-device-api-diagnostic-coding-none-20261008.json)

Target identity: `28dc74803cdee612fd5252d042ac46b759a4e3729147438a6a0a4286ae1e23ab`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.prompt_tokens / coding.metal | 53 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.reused_tokens / coding.metal | 0 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.prefill_tokens / coding.metal | 53 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.generated_tokens / coding.metal | 256 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.proposed_tokens / coding.metal | 300 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.selected_verification_tokens / coding.metal | 298 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.accepted_tokens / coding.metal | 136 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.rejected_tokens / coding.metal | 162 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| turn.discarded_tokens / coding.metal | 2 | count | Server-authored population in one instrumented diagnostic turn; not timed comparison; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/native/receipt.json |
| request_setup.wall_seconds / coding.metal/request_setup | 0.810009797 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.observed_device_seconds / coding.metal/request_setup | 0.043057906 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.api_without_observed_device_seconds / coding.metal/request_setup | 0.747306776 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.outside_both_seconds / coding.metal/request_setup | 0.019645115 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.count / coding.metal/request_setup | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.union_seconds / coding.metal/request_setup | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.kernel.logical_bytes / coding.metal/request_setup | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.copy.count / coding.metal/request_setup | 147 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.copy.union_seconds / coding.metal/request_setup | 0.000115008 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.copy.logical_bytes / coding.metal/request_setup | 1332112 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.memset.count / coding.metal/request_setup | 562 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.memset.union_seconds / coding.metal/request_setup | 0.042942898 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.memset.logical_bytes / coding.metal/request_setup | 8.40774601e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.api.count / coding.metal/request_setup | 3819 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.api.union_seconds / coding.metal/request_setup | 0.79036185 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| request_setup.api.logical_bytes / coding.metal/request_setup | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.wall_seconds / coding.metal/tokenizer | 0.000472944 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.observed_device_seconds / coding.metal/tokenizer | 0 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.api_without_observed_device_seconds / coding.metal/tokenizer | 0 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.outside_both_seconds / coding.metal/tokenizer | 0.000472944 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.count / coding.metal/tokenizer | 0 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.union_seconds / coding.metal/tokenizer | 0 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.kernel.logical_bytes / coding.metal/tokenizer | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.count / coding.metal/tokenizer | 0 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.union_seconds / coding.metal/tokenizer | 0 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.copy.logical_bytes / coding.metal/tokenizer | 0 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.count / coding.metal/tokenizer | 0 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.union_seconds / coding.metal/tokenizer | 0 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.memset.logical_bytes / coding.metal/tokenizer | 0 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.api.count / coding.metal/tokenizer | 0 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.api.union_seconds / coding.metal/tokenizer | 0 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| tokenizer.api.logical_bytes / coding.metal/tokenizer | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.wall_seconds / coding.metal/prefill | 1.23874456 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.observed_device_seconds / coding.metal/prefill | 1.09908127 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.api_without_observed_device_seconds / coding.metal/prefill | 0.045295016 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.outside_both_seconds / coding.metal/prefill | 0.094368268 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.kernel.count / coding.metal/prefill | 2966 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.kernel.union_seconds / coding.metal/prefill | 1.07658939 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.kernel.logical_bytes / coding.metal/prefill | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.copy.count / coding.metal/prefill | 2585 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.copy.union_seconds / coding.metal/prefill | 0.013948275 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.copy.logical_bytes / coding.metal/prefill | 1.13538307e+09 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.memset.count / coding.metal/prefill | 1624 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.memset.union_seconds / coding.metal/prefill | 0.008543609 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.memset.logical_bytes / coding.metal/prefill | 1.48282098e+09 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.api.count / coding.metal/prefill | 13615 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.api.union_seconds / coding.metal/prefill | 1.13978078 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| prefill.api.logical_bytes / coding.metal/prefill | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.wall_seconds / coding.metal/first_decode | 0.59203848 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.observed_device_seconds / coding.metal/first_decode | 0.364777503 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.api_without_observed_device_seconds / coding.metal/first_decode | 0.134945528 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.outside_both_seconds / coding.metal/first_decode | 0.092315449 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.count / coding.metal/first_decode | 5224 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.union_seconds / coding.metal/first_decode | 0.350637939 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.kernel.logical_bytes / coding.metal/first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.copy.count / coding.metal/first_decode | 4354 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.copy.union_seconds / coding.metal/first_decode | 0.011349168 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.copy.logical_bytes / coding.metal/first_decode | 478069000 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.memset.count / coding.metal/first_decode | 3053 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.memset.union_seconds / coding.metal/first_decode | 0.002790396 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.memset.logical_bytes / coding.metal/first_decode | 308637164 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.api.count / coding.metal/first_decode | 22064 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.api.union_seconds / coding.metal/first_decode | 0.494785567 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| first_decode.api.logical_bytes / coding.metal/first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.wall_seconds / coding.metal/post_first_decode | 26.5718224 | s | Clipped phase interval: wall_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.observed_device_seconds / coding.metal/post_first_decode | 21.7901833 | s | Clipped phase interval: observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.api_without_observed_device_seconds / coding.metal/post_first_decode | 2.41572092 | s | Clipped phase interval: api_without_observed_device_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.outside_both_seconds / coding.metal/post_first_decode | 2.36591818 | s | Clipped phase interval: outside_both_seconds; device/API-exclusive/outside partition is disjoint; phase wall is its total; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.count / coding.metal/post_first_decode | 310028 | count | Observed kernel count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.union_seconds / coding.metal/post_first_decode | 21.0256203 | s | Observed kernel union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.kernel.logical_bytes / coding.metal/post_first_decode | NOT MEASURED | byte | Observed kernel logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.count / coding.metal/post_first_decode | 266867 | count | Observed copy count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.union_seconds / coding.metal/post_first_decode | 0.65289931 | s | Observed copy union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.copy.logical_bytes / coding.metal/post_first_decode | 3.048163e+10 | byte | Observed copy logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.count / coding.metal/post_first_decode | 178593 | count | Observed memset count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.union_seconds / coding.metal/post_first_decode | 0.111663634 | s | Observed memset union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.memset.logical_bytes / coding.metal/post_first_decode | 1.4883818e+10 | byte | Observed memset logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.count / coding.metal/post_first_decode | 1090338 | count | Observed api count; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.union_seconds / coding.metal/post_first_decode | 23.9756764 | s | Observed api union_seconds; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |
| post_first_decode.api.logical_bytes / coding.metal/post_first_decode | NOT MEASURED | byte | Observed api logical_bytes; unions of different kinds may overlap; logical bytes are not physical DRAM traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/published-memset-coding-profile-01/device-activity-summary.json |

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
| source_commit | e167b04b5ee342a37cc5e9a249fa3c917223fe03 |
| source_tree | 5c0550e89e4a110064875ed1c325a4354dda49ee |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | aa5e5462563ad3083c663213181a4e577cc324f0f3261ecbf1b1fdc2bc6423e0 |
| executable | 8fb612717bc45dc4cf0669f18ec1fd1fd7b3d9706473866d61be3d81081d1587 |
| backend | cuda |
| backend_implementation | backend.cuda@e167b04b5ee342a37cc5e9a249fa3c917223fe03+e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| kernel_bundle | 7d8704f5c943b735ddf0001b35d74357e4a629aabce948cf8fe48d5753477240 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | e62757fdc3a01f8b0a64f75f79f0c1a7f5077f8d0038e44af5f12615bae11d74 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

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
