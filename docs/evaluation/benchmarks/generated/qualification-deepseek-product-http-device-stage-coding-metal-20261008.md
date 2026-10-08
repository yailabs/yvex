<!-- docs:metadata
title: "GB10 HTTP seeded coding.metal / none"
id: yvex.evaluation.qualification.deepseek-product-http-device-stage-coding-metal-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-product-http-device-stage-coding-metal-20261008.json
-->

# GB10 HTTP seeded coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-product-http-device-stage-coding-metal-20261008.json)

Target identity: `24eeac01d3e4aeb0e3d45a709a5f73bcb23f741fa40a42662f40bab1622e14a7`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| prefill.wall / coding.metal/turn-0 | 1.49566 | s | 3 | 1.48671–1.50148 | 0.005817 |
| ttft.server / coding.metal/turn-0 | 6.56385 | s | 3 | 6.55025–6.69818 | 0.0135946 |
| ttft.client-visible / coding.metal/turn-0 | 7.10428 | s | 3 | 7.09748–7.21339 | 0.00680439 |
| request.client-complete / coding.metal/turn-0 | 258.377 | s | 3 | 258.277–258.569 | 0.0992836 |
| final.first.server / coding.metal/turn-0 | 6.56385 | s | 3 | 6.55025–6.69818 | 0.0135946 |
| final.first.client / coding.metal/turn-0 | 7.10428 | s | 3 | 7.09748–7.21339 | 0.00680439 |
| prefill.uncached / coding.metal/turn-0 | 35.4359 | token/s | 3 | 35.2986–35.6492 | 0.137285 |
| decode.post-first.committed / coding.metal/turn-0 | 1.01644 | token/s | 3 | 1.0159–1.01682 | 0.00037773 |
| final.phase-rate / coding.metal/turn-0 | 1.00022 | token/s | 3 | 0.999196–1.0006 | 0.000382435 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.metal/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 53 | token | 3 | 53–53 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 53 | token | 3 | 53–53 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | NOT MEASURED | cycle | — | — | — |
| Draft forwards | NOT MEASURED | forward | — | — | — |
| Proposed | NOT MEASURED | token | — | — | — |
| Selected verification | NOT MEASURED | token | — | — | — |
| Target verifications | NOT MEASURED | verification | — | — | — |
| Accepted draft | NOT MEASURED | token | — | — | — |
| Rejected draft | NOT MEASURED | token | — | — | — |
| Discarded draft | NOT MEASURED | token | — | — | — |
| Correction/bonus | NOT MEASURED | token | — | — | — |
| Per-sample mean accepted prefix | NOT MEASURED | token | — | — | — |
| Per-sample maximum accepted prefix | NOT MEASURED | token | — | — | — |
| Draft phase | NOT MEASURED | s | — | — | — |
| Verification phase | NOT MEASURED | s | — | — | — |
| Speculative commit phase | NOT MEASURED | s | — | — | — |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not qualified by HTTP timing;  |
| checkpoint-reference | BLOCKED | Not qualified by HTTP timing; No independent full-model quality comparison in this timing series |
| representation-quality | BLOCKED | Not qualified by HTTP timing; No independent full-model quality comparison in this timing series |
| backend-execution | UNQUALIFIED | Not qualified by HTTP timing;  |
| deployment-performance | CHARACTERIZED | Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy;  |
| product-path | CHARACTERIZED | Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy;  |

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
| source_commit | f744f2b84ae71cf9e57f9c65ab23962e0d302dc2 |
| source_tree | 8f55a89cbe4754fcd468cf65e7667148eb538a24 |
| source_delta | ea011118be469307aeb4ab046dd04a56d508c98fd956b93a43cd329b1725f54f |
| build | c19953ae572a337b470dffd982eed348c2fc9cf2ba753d18ecdd29749d807714 |
| executable | 5a37564d084036021500b30f0175dbe66650d16604758d154d3f2ee31967dc1c |
| backend | cuda |
| backend_implementation | backend.cuda@f744f2b84ae71cf9e57f9c65ab23962e0d302dc2+ea011118be469307aeb4ab046dd04a56d508c98fd956b93a43cd329b1725f54f |
| kernel_bundle | 121843052bf4a7d8df5f2d850f807fbabd0b698c8da45281a936eb9f531da99b |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 7274ee0ed679d654c377d605a549ef84d4ebe443ce175afe8212833770dc4058 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"seed":721,"stochastic":true,"temperature":1,"top_p":1} |
| product_path | http-compatibility/Rust-host/native-v25-adapter |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-device-stage-native-01/observation.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Seeded stochastic HTTP control differs from native greedy defaults; no transport-only comparison.
- First-fragment server publication and HTTP admission acknowledgement remain unavailable, not zero.
- Profiled runs are rejected; first post-load and later fresh-session samples are retained without hidden warmup.
- Each HTTP request owns a new ephemeral session; multi-turn message history is not persistent prefix reuse.
- N=3 case/configuration-bound characterization is not 20/700, independent quality or release qualification.
- Bounded output does not imply natural EOS; unreached reasoning/final transition is NOT MEASURED.
- RSS, file mappings and CUDA ownership overlap; no uninterrupted device reservation or cold-load claim.
