<!-- docs:metadata
title: "DeepSeek 0731: protected-service memory audit and operator reboot barrier"
id: yvex.evaluation.qualification.deepseek-0731-memory-audit-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-memory-audit-20261010.json
-->

# DeepSeek 0731: protected-service memory audit and operator reboot barrier

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-memory-audit-20261010.json)

Target identity: `cf323fcb612cf1e88ba9f3ca25af7291be31ed2e18e6b61297700b91a6ada6da`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| system.MemTotal / pre-reboot | 1.30663166e+11 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.MemFree / pre-reboot | 1.01698642e+10 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.MemAvailable / pre-reboot | 1.20348803e+11 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.Cached / pre-reboot | 1.10090727e+11 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.SReclaimable / pre-reboot | 1.07841946e+09 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.SUnreclaim / pre-reboot | 352362496 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.AnonPages / pre-reboot | 7.16088934e+09 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.PageTables / pre-reboot | 247607296 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.KernelStack / pre-reboot | 18907136 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.SwapTotal / pre-reboot | 1.71798651e+10 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.SwapFree / pre-reboot | 1.7016447e+10 | byte | Linux /proc/meminfo; byte classes overlap and are not additive; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Rss / pre-reboot | 1.01221384e+11 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Pss / pre-reboot | 1.01216276e+11 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Pss_Anon / pre-reboot | 5.37389466e+09 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Pss_File / pre-reboot | 9.5798145e+10 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Pss_Shmem / pre-reboot | 44236800 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Swap / pre-reboot | 39665664 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.Locked / pre-reboot | 0 | byte | Installed PID/start-identity smaps rollup; not CUDA allocation or unique memory total; Locked is not driver pinning evidence; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| old-deepseek.file-rss / pre-reboot | 9.49717647e+10 | byte | Observed old-checkpoint artifact mapping resident pages, not new 0731; subset of host RSS and file cache; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| old-deepseek.mapping-size / pre-reboot | 9.50502113e+10 | byte | Virtual mapping size, not an additional allocation; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| finite.file-rss / pre-reboot | 719384576 | byte | Finite source mapping RSS; subset of host/file cache; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.declared-workspace / pre-reboot | 214582212 | byte | Runtime-owned workspace (finite), may overlap anonymous/PSS; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| host.declared-prepared / pre-reboot | 0 | byte | Runtime reports no separate prepared model storage; not proof of no driver/allocator memory; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.payload / pre-reboot | 1.07066195e+11 | byte | Unchanged authenticated 0731 initial preflight arithmetic from prior receipt; not an allocation observed by this audit; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.maximum-tensor-transient / pre-reboot | 1.05906176e+09 | byte | Unchanged authenticated 0731 initial preflight arithmetic from prior receipt; not an allocation observed by this audit; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.system-reserve / pre-reboot | 1.63328957e+10 | byte | Unchanged authenticated 0731 initial preflight arithmetic from prior receipt; not an allocation observed by this audit; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.minimum-preflight / pre-reboot | 1.24458152e+11 | byte | Unchanged authenticated 0731 initial preflight arithmetic from prior receipt; not an allocation observed by this audit; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.deficit-at-sample / pre-reboot | 4.10934937e+09 | byte | Derived initial-preflight minus sampled available memory; no simultaneous second-model admission or fresh load attempt; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| candidate.maximum-other-unavailable / pre-reboot | 6.20501351e+09 | byte | Necessary initial bound only: other unavailable memory must fit here; later workspace/backend/state checks still mandatory; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| cleanup.proven-recovered / pre-reboot | 0 | byte | No attributable abandoned resource established or released; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/pre-reboot.json |
| system.MemAvailable-after / post-audit | 1.20347877e+11 | byte | Second same-boot sample after read-only audit; delta is ambient movement, not cleanup benefit; /home/dgmothx/lab/models/evidence/deepseek-memory-20261010.3bnxnVtu/post-audit.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | CHARACTERIZED | Four official encoding vectors plus artifact-bound BPE and bounded none/high/maximum prompt controls; not full tool/history semantics;  |
| checkpoint-reference | BLOCKED | Exact 0731 source; independent full-model checkpoint reference remains missing; No checkpoint-matched independent full-model output/logit evidence |
| representation-quality | BLOCKED | Native Q8_0/Q2_K artifact integrity is not quantization quality; No independent checkpoint-matched quality comparison for this physical variant |
| backend-execution | BLOCKED | 0731 admission/execution remains blocked; audit preserves installed old DeepSeek and finite; Operator owns reboot; no stop/restart/unload permitted. Initial 0731 envelope remains above sampled available bytes. Reboot recovery and later full workspace/backend admission are unproven. |
| deployment-performance | BLOCKED | 0731 admission/execution remains blocked; audit preserves installed old DeepSeek and finite; Operator owns reboot; no stop/restart/unload permitted. Initial 0731 envelope remains above sampled available bytes. Reboot recovery and later full workspace/backend admission are unproven. |
| product-path | BLOCKED | 0731 admission/execution remains blocked; audit preserves installed old DeepSeek and finite; Operator owns reboot; no stop/restart/unload permitted. Initial 0731 envelope remains above sampled available bytes. Reboot recovery and later full workspace/backend admission are unproven. |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | d66294b5e016806f2e283e8b0160e25385302fa6c9db4ac24a49e94140fa0ea9 |
| representation | deepseek-v4-flash-0731-q8_0-q2_k-v1 |
| artifact_set | 4dc4265a92d77c874c82aa16c1358688b71bb7b10267cc80911c3ef42d2fa11a |
| binding | 29676295361a8c99b86b3e1837a64e23791337f862d01fbeaeb2b5361399cc6d |
| specialization | NOT RETAINED |
| source_commit | 4ecc2b6b4abde7e76e34ba3758b946842f0a04f3 |
| source_tree | 599754283780af35ae3b6bfcda2f5f1448741207 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | 90f1b0db1d8341e7608359d5dc68fa665637f26cd86c773d6922a82311a515ef |
| executable | f3d1611be8a322d080117380d9cf61591f0544173cc20071095d85e5a9c53c72 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | requested 4096 context; native host isolated; no engine admitted |
| context | 4096 |
| prefill_geometry | not admitted; configured CUDA default 512 not reached |
| sequence_geometry | requested width 1; no session created |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none intended; no request submitted |
| sampling | greedy intended; no request submitted |
| product_path | isolated Rust host / native protocol v25 load admission |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No 0731 load or inference executed in this audit. Artifact/source integrity and minimum preflight derive from prior authenticated receipts; this audit does not rehash the full model.
- Memory samples are non-atomic. Host file-backed pages, system cache, PSS, cgroup charges and CUDA observations overlap. Do not add them.
- Protected processes expose RSS but not all PSS/mappings. Driver allocations/pinning and anonymous allocator ownership remain incompletely attributable. No memory leak or safe reclaimable anonymous extent is proved.
- The 190 MiB nvidia-smi process value is not whole UMA model memory. Zero smaps Locked does not refute CUDA registration.
- No abandoned test process established. Nothing killed/deleted/evicted. Boot and installed service/engine generations unchanged; sample delta is not memory recovery.
- Reboot may recover process/driver retention, but does not guarantee admission. If the old DeepSeek automatically returns, do not load a second one or unload it without new authority.
- No new numerical class, prepared layout, runtime speedup, independent model quality, 20/700 result or rollout is qualified.
