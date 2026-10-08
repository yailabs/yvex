<!-- docs:metadata
title: "GB10 HTTP seeded coding.metal / none"
id: yvex.evaluation.qualification.deepseek-product-http-sampling-streams-coding-metal-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-product-http-sampling-streams-coding-metal-20261008.json
-->

# GB10 HTTP seeded coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-product-http-sampling-streams-coding-metal-20261008.json)

Target identity: `70d2adf359b55353a42ce86049f7e9249ce9ac883e36444be27b2a6d83875a65`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| prefill.wall / coding.metal/turn-0 | 1.48947 | s | 3 | 1.48267–1.50541 | 0.00680619 |
| ttft.server / coding.metal/turn-0 | 3.81179 | s | 3 | 3.80684–3.9322 | 0.00494438 |
| ttft.client-visible / coding.metal/turn-0 | 4.28766 | s | 3 | 4.26869–4.40115 | 0.0189641 |
| request.client-complete / coding.metal/turn-0 | 118.011 | s | 3 | 118.005–118.204 | 0.00567656 |
| final.first.server / coding.metal/turn-0 | 3.81179 | s | 3 | 3.80684–3.9322 | 0.00494438 |
| final.first.client / coding.metal/turn-0 | 4.28766 | s | 3 | 4.26869–4.40115 | 0.0189641 |
| prefill.uncached / coding.metal/turn-0 | 35.583 | token/s | 3 | 35.2063–35.7464 | 0.163344 |
| decode.post-first.committed / coding.metal/turn-0 | 2.24973 | token/s | 3 | 2.24864–2.24984 | 0.00010839 |
| final.phase-rate / coding.metal/turn-0 | 2.21321 | token/s | 3 | 2.21017–2.21328 | 6.90185e-05 |

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
| source_delta | 46a490b1955a9fdab3201b615da316b1c97419572a8374c10631fb5d67c00e94 |
| build | 7eaf48756cc9f789fbaf12b081c35b57ead96284fe1273354252090ca5a510a6 |
| executable | 0387c7d1a27ed962bbfb7a5e552e9a9016da2179dc0708f76f547dfffefe10ad |
| backend | cuda |
| backend_implementation | backend.cuda@f744f2b84ae71cf9e57f9c65ab23962e0d302dc2+46a490b1955a9fdab3201b615da316b1c97419572a8374c10631fb5d67c00e94 |
| kernel_bundle | 801e50b81947058473a8be1dccff8ed60f00426e67eb18628a26c76c3dddca7f |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 5766b5f2e91e1458ee8d3d5a92f3b1e14a10e9beed1e591d65b1c45bf6aff3dc |
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

- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-streams-native-01/observation.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Seeded stochastic HTTP control differs from native greedy defaults; no transport-only comparison.
- First-fragment server publication and HTTP admission acknowledgement remain unavailable, not zero.
- Profiled runs are rejected; first post-load and later fresh-session samples are retained without hidden warmup.
- Each HTTP request owns a new ephemeral session; multi-turn message history is not persistent prefix reuse.
- N=3 case/configuration-bound characterization is not 20/700, independent quality or release qualification.
- Bounded output does not imply natural EOS; unreached reasoning/final transition is NOT MEASURED.
- RSS, file mappings and CUDA ownership overlap; no uninterrupted device reservation or cold-load claim.
