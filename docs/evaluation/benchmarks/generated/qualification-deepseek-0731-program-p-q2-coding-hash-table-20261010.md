<!-- docs:metadata
title: "0731 Q2_K: bounded full-model execution, first post-load coding.hash-table"
id: yvex.evaluation.qualification.deepseek-0731-program-p-q2-coding-hash-table-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-q2-coding-hash-table-20261010.json
-->

# 0731 Q2_K: bounded full-model execution, first post-load coding.hash-table

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-q2-coding-hash-table-20261010.json)

Target identity: `007458422e511299c9921731445542f599f12920aedf60dfd2e108dff7a83bf9`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| prefill.wall / coding.hash-table | 1.09125 | s | 1 | 1.09125–1.09125 | 0 |
| prefill.uncached / coding.hash-table | 28.4078 | token/s | 1 | 28.4078–28.4078 | 0 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| prompt.new-positions / coding.hash-table | 31 | count | New input positions; no reused prefix; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| generation.committed / coding.hash-table | 256 | count | Committed output positions, not proposals; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| generation.wall / coding.hash-table | 28.6901752 | s | Complete direct generation, excluding engine load and process cleanup; not HTTP/native-client turn timing; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| forward.subsequent / coding.hash-table | 25.7577513 | s | Subsequent target forward spans only; exclude readout/sampling/publication; NOT sustained committed decode denominator; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| execution.kernel-launches / coding.hash-table | 619521 | count | Runtime execution counter over complete prompt and generation; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| execution.graph-launches / coding.hash-table | 22102 | count | Runtime execution counter over complete prompt and generation; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| execution.queue-synchronizations / coding.hash-table | 33923 | count | Runtime execution counter over complete prompt and generation; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |
| execution.device-synchronizations / coding.hash-table | 257 | count | Runtime execution counter over complete prompt and generation; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| coding.hash-table | PASS | Declared input/output completed, model state committed, owned process released |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Native emission and pinned independent GGUF reader establish structure, not checkpoint-matched quality; Independent held-out comparison missing |
| backend-execution | CHARACTERIZED | Exact artifact/binding executes full model and commits declared positions; N=1 first post-load control, not a warm-series throughput gate;  |
| deployment-performance | CHARACTERIZED | Exact artifact/binding executes full model and commits declared positions; N=1 first post-load control, not a warm-series throughput gate;  |
| product-path | UNQUALIFIED | Direct native engineering entrypoint; no native-protocol/HTTP/client-visible timing in this record;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | 82a4b9ac5e72f5068461f9c5c1efa7b2264fc183c3da1b8ed5bc9901d7fea6df |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 1e1d143a28c02e50a29deb8aa1123eb4d16866e8e2d348aa1d7081f1a9e18da2 |
| representation | goal-v1-q2_k; family-preserved roles unchanged |
| artifact_set | 7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f |
| binding | a4bb99c92e1f8d1fb61f7db3ed0fc5d7a900fcc87b7ecd6fd8c75c486a939737 |
| specialization | 07c20df0a3953d38b9e233817c811a2ff49a80aeeb2c29964e6d3bde002ff234 |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 0954ac9646d5d4a98dd2e460e7656c995ef27e0b1ad9e31548d1c03a056601c4 |
| build | 43b7e711cd984715151d7841f65fbd8270cf0e00589862a5bfb0d58fc0d08421 |
| executable | 9e4f3ea7c4f9b1fb942774117bfa6df0e405989d3a680fadc5471f1425c3b291 |
| backend | cuda |
| backend_implementation | CUDA homogeneous Q2_K routed matrix + encoded row; declared existing arithmetic |
| kernel_bundle | 4a21492217e459a8c6391d6dfd068d8154f704186249bbb64e67a5cba173a6ad |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated first generation after new engine load; no prefix reuse; source-stable candidate, not installed Host |
| context | 4096 |
| prefill_geometry | chunk 512; actual new positions retained in diagnostics |
| sequence_geometry | requested width 1; fresh isolated execution |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature 1; no stochastic draws |
| product_path | direct native engineering generation; not resident-host/native-protocol performance |
| suite | deepseek-0731-competitive@2026-10-09.1 |

## Definitions and reproducibility

- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: controlled first request after new engine load; no prefix reuse; N=1 characterization, not warm-engine repeated qualification or product-native latency. Session: fresh; warm/cold: first request after engine load; authentication reads file; no cold-file or warmed-engine claim; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: controlled first request after new engine load; no prefix reuse; N=1 characterization, not warm-engine repeated qualification or product-native latency. Session: fresh; warm/cold: first request after engine load; authentication reads file; no cold-file or warmed-engine claim; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-controlled-coding.hash-table/result.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One first-post-load sample: no dispersion or sustained qualification gate from this record.
- Target-only none only; reasoning high/maximum, speculation and hosted product are separate evidence.
- Independent checkpoint continuation, held-out quantization quality and the 20/700 performance exit remain unearned.
- The internal subsequent_decode phase excludes readout/sampling; no committed decode rate is derived from it.
- Attention/MoE host phase spans overlap and must not be summed as disjoint critical-path costs.
- Source capture includes six retained CUDA candidate edits; this is not a clean published or installed build.
- Full-model admission does not make the representation a qualified planner recommendation.
- Installed Host/listeners remained unchanged and empty; no installed model generation was created.
