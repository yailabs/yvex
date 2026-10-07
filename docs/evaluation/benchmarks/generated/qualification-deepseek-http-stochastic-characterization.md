<!-- docs:metadata
title: "DeepSeek HTTP default-stochastic characterization"
id: yvex.evaluation.qualification.deepseek-http-stochastic-characterization
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-http-stochastic-characterization.json
-->

# DeepSeek HTTP default-stochastic characterization

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-http-stochastic-characterization.json)

Target identity: `1519a5019000c57289de52f3f4fc98ccfe3f40a1fe36615db276c1fac9797d15`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| request.client-complete / coding.metal/turn-0 | 288.188 | s | 1 | 288.188–288.188 | 0 |
| ttft.client-visible / coding.metal/turn-0 | 8.49066 | s | 1 | 8.49066–8.49066 | 0 |
| ttft.server / coding.metal/turn-0 | 6.87947 | s | 1 | 6.87947–6.87947 | 0 |
| prefill.uncached / coding.metal/turn-0 | 32.5663 | token/s | 1 | 32.5663–32.5663 | 0 |
| prefill.wall / coding.metal/turn-0 | 1.62745 | s | 1 | 1.62745–1.62745 | 0 |
| decode.post-first.committed / coding.metal/turn-0 | 0.91384 | token/s | 1 | 0.91384–0.91384 | 0 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No inherited qualification;  |
| checkpoint-reference | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| representation-quality | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| backend-execution | UNQUALIFIED | Backend qualification is not inferred from a completed performance sample;  |
| deployment-performance | CHARACTERIZED | Single HTTP compatibility request, default stochastic sampling; not native-chat or transport-only comparison;  |
| product-path | CHARACTERIZED | Single HTTP compatibility request, default stochastic sampling; not native-chat or transport-only comparison;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | mixed-iq2xxs-q2k-mxfp4-v1 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | ebdbff6d9571be7ee8032241de315e091d6d560cf72198a6952770028b069cdb |
| build | 048f8d1106e40fe318f8d2b8ba556dde1d5fac782bc07b1e2c0586cacdf45370 |
| executable | 31fd1c9d729eaeaae7d18a341123d53f5e2c312a751f82687bb7c1ee8ecf7c45 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | NOT RETAINED |
| memory_configuration | NOT RETAINED |
| runtime_configuration | NOT RETAINED |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0,"seed":null,"seed_origin":"server-generated-not-projected","stochastic":true,"temperature":1,"top_k":0,"top_p":1,"typical_p":1} |
| product_path | http-openai-compatibility |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **request.client-complete**: client dispatch including connect through terminal response. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: HTTP client and correlation-bound producer phases; one sample only. Session: fresh; warm/cold: resident engine; no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/http-candidate-01/case-0/observations.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- One sample is not a stable performance qualification or throughput gate.
- Default HTTP stochastic policy differs from deterministic native defaults; the latency difference is not transport overhead.
- Server-generated seed was not retained; no deterministic continuation/reproduction claim.
- Same executable and weights do not establish quality. Independent reference remains missing.
- No high/maximum reasoning, long context, 2K/8K prefill or target-only qualification.
- HTTP completion metrics count model-emitted channels; no hidden reasoning is exposed.
