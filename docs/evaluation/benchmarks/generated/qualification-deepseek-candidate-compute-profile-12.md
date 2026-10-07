<!-- docs:metadata
title: "DeepSeek candidate \u2014 diagnostic whole-model CUDA submission profile"
id: yvex.evaluation.qualification.deepseek-candidate-compute-profile-12
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-candidate-compute-profile-12.json
-->

# DeepSeek candidate — diagnostic whole-model CUDA submission profile

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-candidate-compute-profile-12.json)

Target identity: `73e34696a0900a7f388f46e7f6282aa50b57244b39a8265d18ec932d0486cf4b`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-client / engine-load | 8.37830547 | s | Client load request to ready; file warming is separately reported, N=1, not a repeated load benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/identity.json |
| weight-warming / engine-load | NOT MEASURED | s | Explicit one-byte-per-page warming before load; null when not selected; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/identity.json |
| prefill-wall / prefill | 1.46355778 | s | Server-authored phase span under CUPTI instrumentation; diagnostic, not timed benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| prefill-kernel-union / prefill | 0.759026637 | s | Union of clipped device kernel intervals; overlaps API waits and must not be added to them; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| prefill-kernel-count / prefill | 4585 | count | Actual clipped kernel activities in this phase; no extrapolation across other workloads; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| prefill-cuCtxSynchronize / prefill | 827 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| prefill-cuStreamSynchronize / prefill | 217 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| prefill-cuMemcpyDtoH_v2 / prefill | 474 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-wall / first_decode | 0.146164515 | s | Server-authored phase span under CUPTI instrumentation; diagnostic, not timed benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-kernel-union / first_decode | 0.08720161 | s | Union of clipped device kernel intervals; overlaps API waits and must not be added to them; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-kernel-count / first_decode | 2547 | count | Actual clipped kernel activities in this phase; no extrapolation across other workloads; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-cuCtxSynchronize / first_decode | 498 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-cuStreamSynchronize / first_decode | 218 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| first_decode-cuMemcpyDtoH_v2 / first_decode | 475 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-wall / post_first_decode | 40.7938739 | s | Server-authored phase span under CUPTI instrumentation; diagnostic, not timed benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-kernel-union / post_first_decode | 22.2231965 | s | Union of clipped device kernel intervals; overlaps API waits and must not be added to them; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-kernel-count / post_first_decode | 625413 | count | Actual clipped kernel activities in this phase; no extrapolation across other workloads; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-cuCtxSynchronize / post_first_decode | 121125 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-cuStreamSynchronize / post_first_decode | 55590 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |
| post_first_decode-cuMemcpyDtoH_v2 / post_first_decode | 121125 | count | Observed CUDA Driver calls; API elapsed time overlaps device work, not an additive CPU overhead estimate; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-compute-profile-12/profile-summary-v2.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | One complete 31-input/256-output target-only execution under CUPTI with zero dropped records; not numerical qualification;  |
| checkpoint-reference | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |
| deployment-performance | UNQUALIFIED | Instrumented diagnostic only; no throughput publication or roofline claim;  |
| family-conformance | UNQUALIFIED | Explicit engineering configuration measured through native protocol v24, not ordinary product defaults or complete model qualification;  |
| product-path | UNQUALIFIED | Instrumented diagnostic only; no throughput publication or roofline claim;  |
| representation-quality | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |

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
| source_commit | 803dd98d4c54d7a26cb3c08def6b350be51b7179 |
| source_tree | 4ae815ad1669b4c95c302bb9b776e3671ee8f0a4 |
| source_delta | f0fedb271f549473263810ba403ca1a9c4652b3ad43df86a210df21bdf1d9754 |
| build | 6ece97e98ac222ef46ac24bc437d95edf728b4e39194850ee59764058ebaf136 |
| executable | af0bb3a77a62ef2b77280f7aa4d4f4c6edc0265b813533764915a7a589e65dae |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 2184fa8e145f9b9e4a7f1cebca4adb2bef60e9388460339b72a14e4b9924acbb |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine/native-v24 |
| suite | 94f29d3a9002bfc3b2d91d3427268099ce7659430d32030c9747e1bf00582a01 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not YVEX-published qualification or a model-quality claim.
- Host source/build/checkpoint/hardware provenance unavailable; unknown fields refuse comparison.
- First fragment publication and reasoning-to-final transition not measured.
- Different generated histories are separate input groups, never pooled or treated as deterministic agreement.
- Advisory locks do not prove hardware exclusivity; external resource observation is required.
- No independent oracle; source stability unknown; first sample is not relabeled warmed by hidden work.
- Controlled-engine authority using native protocol v24: explicit target-only, chunk 512, fresh sessions, temperature 0. Not the restored ordinary speculative product configuration.
- Base commit is accompanied by the frozen dirty delta and exact captured executable. This is not a clean published-source qualification.
- File-page cache state is observed separately from mapping/addressability/device allocation. A already-resident page-touch arm is not a cold-prefetch causal experiment.
- Backend/kernel/runtime-toolkit identities unavailable in this import remain null; no passing timing capture promotes numerical or quality qualification.
- A separate 8K repeat on this source/executable reported invalid/non-finite CUDA MoE status; candidate-wide correctness and publication are not earned.
- One profiled diagnostic turn, not a timed performance sample or quality gate.
- API waits overlap device execution; sums are not additive and kernel time is not bandwidth utilization.
- Sampled CPU-process faults and backing I/O do not prove absence of GPU address-translation faults.
- RSS snapshots do not establish page locking or future residency under memory pressure.
- OS sampling overhead and profiler overhead are retained; no rate from this run is a benchmark claim.
- The separately rejected run 10 detected an overlapping CUDA diagnostic context and was cancelled; its raw evidence is not included as a performance sample.
- No claimed throughput improvement, occupancy percentage, compulsory-bandwidth roofline or proof that all observed synchronization barriers are removable.
