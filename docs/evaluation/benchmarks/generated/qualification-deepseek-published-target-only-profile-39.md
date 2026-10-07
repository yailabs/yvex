<!-- docs:metadata
title: "DeepSeek clean published target-only coding profile; diagnostic only"
id: yvex.evaluation.qualification.deepseek-published-target-only-profile-39
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-published-target-only-profile-39.json
-->

# DeepSeek clean published target-only coding profile; diagnostic only

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-published-target-only-profile-39.json)

Target identity: `f9c64fd420400aec5ddba19fe90e92fef8b5a84ccc641c5ccef185c1dd7c6490`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| request_setup-wall / request_setup | 0.378423816 | s | Server-authored phase span under instrumentation; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| request_setup-kernel-union / request_setup | 0 | s | Union of clipped GPU kernel intervals; overlaps API waits, not occupancy or bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| request_setup-kernel-count / request_setup | 0 | count | Actual intersecting kernel activities in this phase, including graph nodes; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| request_setup-driver-call-count / request_setup | 8043 | count | Observed intersecting Driver API activities, not additional CPU work; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| request_setup-cuCtxSynchronize-count / request_setup | 1406 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| request_setup-cuCtxSynchronize-wait / request_setup | 0.015268945 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| tokenizer-wall / tokenizer | 0.000458608 | s | Server-authored phase span under instrumentation; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| tokenizer-kernel-union / tokenizer | 0 | s | Union of clipped GPU kernel intervals; overlaps API waits, not occupancy or bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| tokenizer-kernel-count / tokenizer | 0 | count | Actual intersecting kernel activities in this phase, including graph nodes; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| tokenizer-driver-call-count / tokenizer | 0 | count | Observed intersecting Driver API activities, not additional CPU work; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-wall / prefill | 1.1603008 | s | Server-authored phase span under instrumentation; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-kernel-union / prefill | 0.752246966 | s | Union of clipped GPU kernel intervals; overlaps API waits, not occupancy or bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-kernel-count / prefill | 4585 | count | Actual intersecting kernel activities in this phase, including graph nodes; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-driver-call-count / prefill | 16740 | count | Observed intersecting Driver API activities, not additional CPU work; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuCtxSynchronize-count / prefill | 611 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuCtxSynchronize-wait / prefill | 0.414776383 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuStreamSynchronize-count / prefill | 131 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuStreamSynchronize-wait / prefill | 0.27203559 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuMemFree_v2-count / prefill | 129 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuMemFree_v2-wait / prefill | 0.071395212 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuGraphInstantiateWithFlags-count / prefill | 86 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| prefill-cuGraphInstantiateWithFlags-wait / prefill | 0.014179826 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-wall / first_decode | 0.142071189 | s | Server-authored phase span under instrumentation; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-kernel-union / first_decode | 0.086411598 | s | Union of clipped GPU kernel intervals; overlaps API waits, not occupancy or bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-kernel-count / first_decode | 2547 | count | Actual intersecting kernel activities in this phase, including graph nodes; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-driver-call-count / first_decode | 11145 | count | Observed intersecting Driver API activities, not additional CPU work; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuCtxSynchronize-count / first_decode | 239 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuCtxSynchronize-wait / first_decode | 0.056576123 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuStreamSynchronize-count / first_decode | 133 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuStreamSynchronize-wait / first_decode | 0.024311955 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuGraphInstantiateWithFlags-count / first_decode | 86 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuGraphInstantiateWithFlags-wait / first_decode | 0.008715008 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuMemFree_v2-count / first_decode | 129 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| first_decode-cuMemFree_v2-wait / first_decode | 0.006410625 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-wall / post_first_decode | 31.2111045 | s | Server-authored phase span under instrumentation; not benchmark timing; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-kernel-union / post_first_decode | 22.0686444 | s | Union of clipped GPU kernel intervals; overlaps API waits, not occupancy or bandwidth; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-kernel-count / post_first_decode | 625413 | count | Actual intersecting kernel activities in this phase, including graph nodes; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-driver-call-count / post_first_decode | 1987401 | count | Observed intersecting Driver API activities, not additional CPU work; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuCtxSynchronize-count / post_first_decode | 55080 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuCtxSynchronize-wait / post_first_decode | 14.3752874 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuStreamSynchronize-count / post_first_decode | 33915 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuStreamSynchronize-wait / post_first_decode | 6.23400541 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuMemFree_v2-count / post_first_decode | 32895 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuMemFree_v2-wait / post_first_decode | 1.62506165 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuGraphInstantiateWithFlags-count / post_first_decode | 273 | count | Observed intersecting Driver calls; no claim that every wait is removable; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |
| post_first_decode-cuGraphInstantiateWithFlags-wait / post_first_decode | 0.044781734 | s | Clipped API time overlaps GPU execution and other waits; do not add to kernel intervals; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/published-physical-profile-v39/target-only/profile-with-admission.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |
| checkpoint-reference | UNQUALIFIED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |
| deployment-performance | UNQUALIFIED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |
| family-conformance | UNQUALIFIED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |
| product-path | UNQUALIFIED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |
| representation-quality | UNQUALIFIED | One source-frozen 31-input/256-output coding profile; no numerical, quality or performance qualification;  |

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
| source_commit | a444bcdd384f6abfc79b07d1a26d93c17c4a97c0 |
| source_tree | 508e7cfeb5887fc123e200038d073ce72dc79049 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | f748ad6114858e5f1b6246f8bd2d6ade9a1743bd72e8e27b351d951c35b00bac |
| executable | e1ea8e02ad223a3fffb2ecc6839ee354658bd97a6e23c7e6b0651007d4ad3b4d |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NOT RETAINED |
| device_count | NOT RETAINED |
| topology | NOT RETAINED |
| driver | NOT RETAINED |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | b1d4e763aefe8e4bd5d7e36c2c41dd1e21a144e58c268c644b3f9a9e19310b56 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy |
| product_path | controlled-engine/native-v25 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One profiled diagnostic turn; no timing from this run is an unprofiled performance claim.
- API waits overlap device execution; clipped sums are not additive.
- Kernel union is activity, not occupancy, Tensor Core utilization or measured memory bandwidth.
- Linux smaps was observed only before and after coding, not sampled inside this turn.
- Request setup spans server receive to turn start; it includes queue, policy, prompt preparation and generation-context open, not transport latency alone.
- N=1 diagnostic per strategy, not a throughput series; measurements is intentionally empty.
- Same source/artifact/binding and coding text, but target-only and DSpark execute different admitted populations.
- Hardware/kernel/toolkit dimensions unavailable from the producer remain null; strict performance comparison refuses.
- No hidden warming, page-cache eviction, alternative representation or numerical relaxation.
- Finite generation 1 and public listeners remained available; DeepSeek restored at generation 3 with unchanged identities/configuration.
- No independent checkpoint/representation-quality or 20/700 gate is earned.
