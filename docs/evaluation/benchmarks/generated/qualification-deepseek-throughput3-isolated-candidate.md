<!-- docs:metadata
title: "DeepSeek isolated synthetic candidate - not local product"
id: yvex.evaluation.qualification.deepseek-throughput3-isolated-candidate
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-throughput3-isolated-candidate.json
-->

# DeepSeek isolated synthetic candidate - not local product

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-throughput3-isolated-candidate.json)

Target identity: `a49ae9fd5233039dc6335f3c7df8fea65761957aa717e1a9765cf8441fd426be`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| request.client-complete / historical-synthetic.decode | 35.4965 | s | 2 | 35.4081–35.5849 | 0.0883663 |
| decode.post-first.committed / historical-synthetic.decode | 7.98012 | token/s | 2 | 7.97753–7.98271 | 0.00258672 |
| ttft.server / historical-synthetic.decode | 1.37319 | s | 2 | 1.37187–1.37451 | 0.00132414 |
| ttft.client-visible / historical-synthetic.decode | 2.79555 | s | 2 | 2.77956–2.81153 | 0.0159854 |
| request.client-complete / historical-synthetic.prefill2k | 26.9085 | s | 2 | 26.7877–27.0294 | 0.120856 |
| prefill.uncached / historical-synthetic.prefill2k | 84.3156 | token/s | 2 | 83.9645–84.6667 | 0.351111 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No qualification imported from this measurement;  |
| checkpoint-reference | BLOCKED | No qualification imported from this measurement; Independent exact-checkpoint full-model reference and representation comparison absent |
| representation-quality | BLOCKED | No qualification imported from this measurement; Independent exact-checkpoint full-model reference and representation comparison absent |
| backend-execution | UNQUALIFIED | Backend qualification is not inferred from a completed performance sample;  |
| deployment-performance | CHARACTERIZED | Two repeated synthetic controls on an isolated engineering profile over HTTP; not local chat;  |
| product-path | CHARACTERIZED | Two repeated synthetic controls on an isolated engineering profile over HTTP; not local chat;  |

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
| source_delta | d16c9792b008fb3772397891479ce0f06e69976faae291a09d0ffbdea2cc7575 |
| build | 80d30c8d345f3ced7645d78a7760490e1e49d788f8650452e207e144be9d438e |
| executable | c44516dc2c9f9d2d03acfd052defb7e58b0e98861744a387821f25e3c5a43a98 |
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
| context | 32768 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | temperature=0 |
| product_path | controlled-engine-via-http |
| suite | historical-synthetic-throughput3; harness-sha256:998caf0468e6dba6f02365cc41f756fe810eae3fd04b27c6459b553d144e7a3e |

## Definitions and reproducibility

- **request.client-complete**: client dispatch including connect through terminal response. Scope: HTTP client. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#decode.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: server-authored phase. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#decode.
- **ttft.server**: turn start to first committed model-token callback. Scope: server-authored phase. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#decode.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: HTTP client. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#decode.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: HTTP client. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 4. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#prefill2k.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: server-authored phase. Session: fresh; warm/cold: resident engine after four-token primer; output bound: 4. Evidence: /home/dgmothx/lab/models/evidence/yvex-throughput3-20261003.uaMI7e/prefix-target.jsonl#prefill2k.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not native local-product performance: operator used context 4096/chunk 64/speculative.
- Synthetic controls only; representative suite and reasoning matrix not executed.
- Two samples, no release benchmark or independent full-model/quantization quality claim.
- Missing target facts remain null; comparisons requiring them refuse.
- 20 decode / 700 prefill targets are not earned.
