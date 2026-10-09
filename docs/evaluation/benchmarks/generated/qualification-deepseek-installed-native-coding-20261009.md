<!-- docs:metadata
title: "Installed GB10 native coding / 32K context / chunk 64 / generation 44"
id: yvex.evaluation.qualification.deepseek-installed-native-coding-20261009
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-installed-native-coding-20261009.json
-->

# Installed GB10 native coding / 32K context / chunk 64 / generation 44

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-installed-native-coding-20261009.json)

Target identity: `13b1c5bf203ad5e0c139fb7609d60b89efca6e9cf5681ac84221bdcd9d9b2349`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.66636 | s | 3 | 0.645053–0.679335 | 0.0129746 |
| request.client-complete / coding.metal/turn-0 | 32.5544 | s | 3 | 32.4436–33.83 | 0.110842 |
| ttft.client-visible / coding.metal/turn-0 | 2.6726 | s | 3 | 2.60177–2.91441 | 0.0708242 |
| ttft.server / coding.metal/turn-0 | 1.99298 | s | 3 | 1.95656–2.24802 | 0.036419 |
| final.first.server / coding.metal/turn-0 | 1.99298 | s | 3 | 1.95656–2.24802 | 0.036419 |
| final.first.client / coding.metal/turn-0 | 2.6726 | s | 3 | 2.60177–2.91441 | 0.0708242 |
| final.phase-rate / coding.metal/turn-0 | 8.41908 | token/s | 3 | 8.12021–8.42344 | 0.0043602 |
| decode.post-first.committed / coding.metal/turn-0 | 8.53379 | token/s | 3 | 8.24852–8.54529 | 0.0115055 |
| prefill.uncached / coding.metal/turn-0 | 36.131 | token/s | 3 | 32.4015–37.6959 | 1.56487 |
| prefill.wall / coding.metal/turn-0 | 1.46688 | s | 3 | 1.40599–1.63573 | 0.0608948 |

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
| Draft cycles | 60 | cycle | 3 | 60–60 | 0 |
| Draft forwards | 60 | forward | 3 | 60–60 | 0 |
| Proposed | 300 | token | 3 | 300–300 | 0 |
| Selected verification | 298 | token | 3 | 298–298 | 0 |
| Target verifications | 60 | verification | 3 | 60–60 | 0 |
| Accepted draft | 136 | token | 3 | 136–136 | 0 |
| Rejected draft | 162 | token | 3 | 162–162 | 0 |
| Discarded draft | 2 | token | 3 | 2–2 | 0 |
| Correction/bonus | 60 | token | 3 | 60–60 | 0 |
| Per-sample mean accepted prefix | 2.26667 | token | 3 | 2.26667–2.26667 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.50106 | s | 3 | 2.49921–2.54268 | 0.00184627 |
| Verification phase | 15.9326 | s | 3 | 15.9153–16.4762 | 0.017266 |
| Speculative commit phase | 11.4663 | s | 3 | 11.4376–11.9876 | 0.0286938 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | No independent evidence earned by this timing capture;  |
| checkpoint-reference | BLOCKED | No independent evidence earned by this timing capture; No independent model-quality oracle in this timing series |
| deployment-performance | CHARACTERIZED | Installed native request observation; not HTTP or REPLAI paint timing;  |
| family-conformance | UNQUALIFIED | No independent evidence earned by this timing capture;  |
| product-path | CHARACTERIZED | Installed native request observation; not HTTP or REPLAI paint timing;  |
| representation-quality | BLOCKED | No independent evidence earned by this timing capture; No independent model-quality oracle in this timing series |

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
| source_commit | 7fc562d5da05bba104d383266e72e3f8e075e404 |
| source_tree | 0f1794b4c783a6d10d455930f5dea3484fa180bf |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | d22503df96b3ee7b0317cb0abc9cd7ce29891b8c5e1af4c8f2fc935344dc947b |
| executable | fce0ebc0f1e370d9dad12a635134c0d5857883cd935bbc54956f2eeea0bbc19d |
| backend | cuda |
| backend_implementation | backend.cuda@7fc562d5da05bba104d383266e72e3f8e075e404 |
| kernel_bundle | 9ee0e3b3e4c2580eecd1d726adf208090d8084dc67e67272187a78a87d2e27a9 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 1f4b2286d3f0b52a67b14f63e25bed1f1fd3b729fc77df1516534dc2d72e8915 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/installed-native-coding-20261009-flow-window-02/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Three fresh native sessions; exact coding.metal workload, 53 prompt and 256 output tokens, explicit greedy sampling and reasoning none.
- Installed context 32768/chunk 64/speculative; not interchangeable with context 16384/chunk 512 candidate experiments.
- Short-prompt prefill is not a canonical 2K/8K throughput gate.
- No measured load latency or independently qualified checkpoint/quantization quality.
- Existing detached operator session preserved throughout; sampled accelerator observations cannot prove uninterrupted exclusive reservation.
- Pre-sample compiler absence was checked; no continuous compiler reservation was established.
- Hardware clocks and thermal observation were captured after sampling, not throughout it.
- Server first-fragment publication is unavailable; server TTFT and client visibility have distinct clock origins and admission scope.
- Retained source/build capture is shared provenance only; no finite-model evidence is inherited.
- Process RSS, mapped bytes and CUDA ownership overlap and are not summed as physical allocations.
