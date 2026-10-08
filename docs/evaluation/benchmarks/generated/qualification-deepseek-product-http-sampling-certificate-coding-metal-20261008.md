<!-- docs:metadata
title: "GB10 HTTP seeded coding.metal / none"
id: yvex.evaluation.qualification.deepseek-product-http-sampling-certificate-coding-metal-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-product-http-sampling-certificate-coding-metal-20261008.json
-->

# GB10 HTTP seeded coding.metal / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-product-http-sampling-certificate-coding-metal-20261008.json)

Target identity: `185c55bb2c00ea0bd3fe8d964e6d601983cf2812c144481f40d7adf43b4dac8b`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| prefill.wall / coding.metal/turn-0 | 1.49047 | s | 3 | 1.479–1.50355 | 0.0114657 |
| ttft.server / coding.metal/turn-0 | 2.65174 | s | 3 | 2.64441–2.78718 | 0.0073316 |
| ttft.client-visible / coding.metal/turn-0 | 3.13369 | s | 3 | 3.11669–3.24612 | 0.0170075 |
| request.client-complete / coding.metal/turn-0 | 49.943 | s | 3 | 49.7364–49.9837 | 0.0406563 |
| final.first.server / coding.metal/turn-0 | 2.65174 | s | 3 | 2.64441–2.78718 | 0.0073316 |
| final.first.client / coding.metal/turn-0 | 3.13369 | s | 3 | 3.11669–3.24612 | 0.0170075 |
| prefill.uncached / coding.metal/turn-0 | 35.5593 | token/s | 3 | 35.2498–35.835 | 0.275667 |
| decode.post-first.committed / coding.metal/turn-0 | 5.49082 | token/s | 3 | 5.49036–5.50869 | 0.00045564 |
| final.phase-rate / coding.metal/turn-0 | 5.37743 | token/s | 3 | 5.36372–5.39497 | 0.0137078 |

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
| source_delta | 17e81edbd47a09cc9778f9440a1f92941c538ee1c17745a0b80c367889d023bb |
| build | 998933fa838bcf7a3ee80414d7ec095f31cd649755c11aceb25fec556740e7a2 |
| executable | 397ed25c0224b3567121f282ca021b91df11a02bc9d440b3796028477e7bc2da |
| backend | cuda |
| backend_implementation | backend.cuda@f744f2b84ae71cf9e57f9c65ab23962e0d302dc2+17e81edbd47a09cc9778f9440a1f92941c538ee1c17745a0b80c367889d023bb |
| kernel_bundle | 1c5f6e8bf2259f22a896750c3842d77b4e3b8c1bc927e235523099234343734c |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 03a2849788ba43a8032c510395421a656f96b9deb1e29b0235e603e3b0014f3f |
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

- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **ttft.server**: turn start to first committed model-token callback. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **final.first.server**: server turn start to first source-classified final token. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: Frozen Rust-host HTTP compatibility request; seeded stochastic control, not native default policy. Session: fresh; warm/cold: Post-load resident engine; no post-load conditioning; file-cache state uncontrolled; first 0 repetitions excluded explicitly; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/http-sampling-certificate-native-01/observation.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Seeded stochastic HTTP control differs from native greedy defaults; no transport-only comparison.
- First-fragment server publication and HTTP admission acknowledgement remain unavailable, not zero.
- Profiled runs are rejected; first post-load and later fresh-session samples are retained without hidden warmup.
- Each HTTP request owns a new ephemeral session; multi-turn message history is not persistent prefix reuse.
- N=3 case/configuration-bound characterization is not 20/700, independent quality or release qualification.
- Bounded output does not imply natural EOS; unreached reasoning/final transition is NOT MEASURED.
- RSS, file mappings and CUDA ownership overlap; no uninterrupted device reservation or cold-load claim.
