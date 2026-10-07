<!-- docs:metadata
title: "DeepSeek candidate \u2014 CUDA host registration and OS residency diagnostic"
id: yvex.evaluation.qualification.deepseek-candidate-registration-residency-08
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-candidate-registration-residency-08.json
-->

# DeepSeek candidate — CUDA host registration and OS residency diagnostic

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-candidate-registration-residency-08.json)

Target identity: `e3520bf4479725fb263adda4cc8afe6019dcb5af27e66be53e2f9c723a762b51`. Origin: **local**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-client / model-load | 85.7890384 | s | Client load admission through execution-ready engine under callback instrumentation; N=1, not repeated benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/identity.json |
| host-registration / model-load | 81.5847539 | s | Actual successful CUDA Driver API interval, not an inferred phase denominator; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/cupti.tsv |
| registered-host-extent / model-load | 9.50502103e+10 | byte | Host registration extent; not a separate device allocation or proof of permanent pinning; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/cupti.tsv |
| device-address-resolution / model-load | 1.824e-06 | s | Actual successful CUDA Driver API interval, not an inferred phase denominator; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/cupti.tsv |
| mapped-rss-at-load-end / model-load | 9.49192049e+10 | byte | Linux smaps RSS of exact artifact mapping in last load sample; includes file-backed physical pages, not a duplicate CUDA copy; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/load.residency.jsonl |
| process-read-bytes-at-load-end / model-load | 9.48909752e+10 | byte | Linux process I/O read bytes at last load sample; OS observation, not measured GPU memory traffic; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/load.residency.jsonl |
| cpu-major-faults-at-load-end / model-load | 1544 | count | Cumulative CPU major faults at last load sample; not GPU faults; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/load.residency.jsonl |
| file-resident-pages-before / model-load | 58316 | count | Scoped mincore file-cache pages before load; mapped is not resident; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/yvex-residency-registration-08/identity.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | Instrumented registration/OS diagnostic; no throughput, optimality or numerical claim;  |
| checkpoint-reference | BLOCKED | No independent admitted checkpoint-matched quality comparison earned by this timing capture; Full-distribution reference and admitted numerical/quality comparison remain unqualified; successful coding generation is not a quality gate |
| deployment-performance | UNQUALIFIED | Instrumented registration/OS diagnostic; no throughput, optimality or numerical claim;  |
| family-conformance | UNQUALIFIED | Explicit engineering configuration measured through native protocol v24, not ordinary product defaults or complete model qualification;  |
| product-path | UNQUALIFIED | Instrumented registration/OS diagnostic; no throughput, optimality or numerical claim;  |
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
| runtime_configuration | b6893dd7f14cc3cfb5b5fbef746ae553e52a45d20fbc95fcba15bcfe18dd2a46 |
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | controlled-engine/native-v24 |
| suite | 30d56bb8992dffccc0ca9bb269d96c03f99d234e4b04e1bf0279875f01f8ab77 |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- This is a source-stable instrumented diagnostic, not a timed performance comparison or an optimization result.
- The mapped file was almost entirely nonresident before load. Registration coincided with file population; this does not separately decompose filesystem versus registration driver internals.
- No whole-range CUDA pageable prefetch was selected in this arm. API addressability and zero separately allocated model-device bytes do not mean weights are absent from physical RAM.
- Linux smaps Locked is not an authoritative measurement of CUDA driver pinning and is not promoted into that claim.
- No global cache eviction or competing model was used. The isolated host and owned sessions were retired before public producer restoration.
- Checkpoint/representation quality, full 8K correctness, 20/700, and cross-engine checkpoint equivalence are not earned.
