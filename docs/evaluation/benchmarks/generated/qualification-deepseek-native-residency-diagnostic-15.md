<!-- docs:metadata
title: "DeepSeek native residency and launch diagnostics / speculative / none"
id: yvex.evaluation.qualification.deepseek-native-residency-diagnostic-15
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-residency-diagnostic-15.json
-->

# DeepSeek native residency and launch diagnostics / speculative / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-residency-diagnostic-15.json)

Target identity: `7cb0c1e36913b3f42aef7d6a6c07f304a91035d13cdccf4210d3eb5e0e685b86`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| mapped_rss_min_bytes / coding.metal/turn-0 | 9.50502113e+10 | byte | Linux process/file-mapping observation during this profiled turn; not CUDA allocation or GPU paging proof; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| mapped_rss_max_bytes / coding.metal/turn-0 | 9.50502113e+10 | byte | Linux process/file-mapping observation during this profiled turn; not CUDA allocation or GPU paging proof; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| mapped_swap_max_bytes / coding.metal/turn-0 | 0 | byte | Linux process/file-mapping observation during this profiled turn; not CUDA allocation or GPU paging proof; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| process_read_bytes_delta / coding.metal/turn-0 | 0 | byte | Linux process/file-mapping observation during this profiled turn; not CUDA allocation or GPU paging proof; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| cpu_major_fault_delta / coding.metal/turn-0 | 0 | count | Sampled Linux process counter/population; major and minor faults remain distinct; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| cpu_minor_fault_delta / coding.metal/turn-0 | 779968 | count | Sampled Linux process counter/population; major and minor faults remain distinct; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| samples / coding.metal/turn-0 | 37 | count | Sampled Linux process counter/population; major and minor faults remain distinct; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| os-probe-time / coding.metal/turn-0 | 14.3845491 | s | Total time spent reading OS diagnostics; overhead is not hidden; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| prompt_tokens / coding.metal/turn-0 | 53 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| reused_tokens / coding.metal/turn-0 | 0 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| prefill_tokens / coding.metal/turn-0 | 53 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| generated_tokens / coding.metal/turn-0 | 256 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| draft_cycles / coding.metal/turn-0 | 60 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| proposed_tokens / coding.metal/turn-0 | 300 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| accepted_tokens / coding.metal/turn-0 | 136 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| rejected_tokens / coding.metal/turn-0 | 162 | count | Terminal native producer population for this exact diagnostic turn; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-prefill-wall / coding.metal/turn-0/prefill | 1.97795236 | s | Server-event span in a profiled diagnostic, not a timed baseline; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-prefill-kernel-union / coding.metal/turn-0/prefill | 1.17414296 | s | Union of clipped device-kernel intervals; API waits overlap it; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-prefill-kernels / coding.metal/turn-0/prefill | 6620 | count | Device kernel activities intersecting the server span; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-prefill-driver-calls / coding.metal/turn-0/prefill | 22655 | count | Driver activities intersecting the server span; not all calls are synchronizations; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-prefill-context-sync / coding.metal/turn-0/prefill | 833 | count | Observed context synchronization calls; wait time overlaps device execution; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-first_decode-wall / coding.metal/turn-0/first_decode | 0.97004951 | s | Server-event span in a profiled diagnostic, not a timed baseline; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-first_decode-kernel-union / coding.metal/turn-0/first_decode | 0.380388596 | s | Union of clipped device-kernel intervals; API waits overlap it; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-first_decode-kernels / coding.metal/turn-0/first_decode | 5434 | count | Device kernel activities intersecting the server span; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-first_decode-driver-calls / coding.metal/turn-0/first_decode | 26768 | count | Driver activities intersecting the server span; not all calls are synchronizations; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-first_decode-context-sync / coding.metal/turn-0/first_decode | 1331 | count | Observed context synchronization calls; wait time overlaps device execution; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-post_first_decode-wall / coding.metal/turn-0/post_first_decode | 43.5592578 | s | Server-event span in a profiled diagnostic, not a timed baseline; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-post_first_decode-kernel-union / coding.metal/turn-0/post_first_decode | 23.3164728 | s | Union of clipped device-kernel intervals; API waits overlap it; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-post_first_decode-kernels / coding.metal/turn-0/post_first_decode | 326240 | count | Device kernel activities intersecting the server span; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-post_first_decode-driver-calls / coding.metal/turn-0/post_first_decode | 1363253 | count | Driver activities intersecting the server span; not all calls are synchronizations; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |
| profiled-post_first_decode-context-sync / coding.metal/turn-0/post_first_decode | 78675 | count | Observed context synchronization calls; wait time overlaps device execution; /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-residency-profile-speculative-none-15/profile-summary-v2.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | One completed 256-token native coding turn and observed owned cleanup; not independent numerical qualification;  |
| checkpoint-reference | UNQUALIFIED | No qualification of this plane from a single profiled diagnostic;  |
| deployment-performance | UNQUALIFIED | No qualification of this plane from a single profiled diagnostic;  |
| family-conformance | UNQUALIFIED | No qualification of this plane from a single profiled diagnostic;  |
| product-path | CHARACTERIZED | Native protocol v24 request and cleanup observed; profiled wall times are not product benchmark samples;  |
| representation-quality | UNQUALIFIED | No qualification of this plane from a single profiled diagnostic;  |

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
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | e4626c50bb8b3b4d6f2c9cd9fc9614d4582697396ca34f33acc9467a165818d5 |
| build | 8d45003f0647e3675a49f46fa6a02bff367e8f7d971a7ecc7ccd9a37d7f0f7ee |
| executable | cd7413f208f52e26646daf894dac87c3a9e15af385513589ffb2e9dcae83130c |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | 8cbef377dd46d52a267de1996c8f6a7506fffdbd8c1853b898e8f39c45e6fd65 |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One profiled diagnostic turn, not a timed performance sample or quality gate.
- API waits overlap device execution; sums are not additive and kernel time is not bandwidth utilization.
- Sampled CPU-process faults and backing I/O do not prove absence of GPU address-translation faults.
- RSS snapshots do not establish page locking or future residency under memory pressure.
- OS sampling overhead and profiler overhead are retained; no rate from this run is a benchmark claim.
- Mapped RSS is the page-rounded Linux mapping extent; artifact identity and exact payload bytes remain separate.
- No cold load, residency guarantee, bandwidth/occupancy measurement or representation-quality claim is earned.
- All owned work/queue/sessions/clients/leases were zero before supported host stop; this is one recovery observation.
- This record has no quality/performance metrics and cannot supply a throughput baseline or comparison.
