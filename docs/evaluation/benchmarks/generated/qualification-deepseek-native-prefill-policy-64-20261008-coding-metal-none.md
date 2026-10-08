<!-- docs:metadata
title: "GB10 Rust/native chunk 64 / coding / DSpark / none"
id: yvex.evaluation.qualification.deepseek-native-prefill-policy-64-20261008-coding-metal-none
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-prefill-policy-64-20261008-coding-metal-none.json
-->

# GB10 Rust/native chunk 64 / coding / DSpark / none

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-prefill-policy-64-20261008-coding-metal-none.json)

Target identity: `846d24b2e7b48bb5b17682a151406a5e3ce9b5268d2270129496eeef082411d5`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.46783 | s | 3 | 0.442381–0.472239 | 0.00440848 |
| request.client-complete / coding.metal/turn-0 | 27.9632 | s | 3 | 27.9072–28.2331 | 0.0559865 |
| ttft.client-visible / coding.metal/turn-0 | 2.14954 | s | 3 | 2.13851–2.28833 | 0.0110359 |
| ttft.server / coding.metal/turn-0 | 1.69612 | s | 3 | 1.67715–1.82013 | 0.0189703 |
| final.first.server / coding.metal/turn-0 | 1.69612 | s | 3 | 1.67715–1.82013 | 0.0189703 |
| final.first.client / coding.metal/turn-0 | 2.14954 | s | 3 | 2.13851–2.28833 | 0.0110359 |
| final.phase-rate / coding.metal/turn-0 | 9.75131 | token/s | 3 | 9.65007–9.76536 | 0.0140579 |
| decode.post-first.committed / coding.metal/turn-0 | 9.87872 | token/s | 3 | 9.82874–9.89598 | 0.0172576 |
| prefill.uncached / coding.metal/turn-0 | 42.839 | token/s | 3 | 42.4366–42.8937 | 0.0547307 |
| prefill.wall / coding.metal/turn-0 | 1.23719 | s | 3 | 1.23561–1.24892 | 0.00157861 |

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
| Draft phase | 2.26959 | s | 3 | 2.25197–2.3636 | 0.0176256 |
| Verification phase | 15.6896 | s | 3 | 15.6735–15.9496 | 0.0160853 |
| Speculative commit phase | 7.82981 | s | 3 | 7.76043–7.83496 | 0.00514892 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| load-complete / coding.metal/turn-0 | 5.95882296 | s | Single post-cache load-to-ready observation; cache uncontrolled, not a cold-load or repeated load benchmark; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/load.json |
| process-rss-peak / coding.metal/turn-0 | 9.73516186e+10 | byte | Process-lifetime RSS peak across the three requests; includes file-backed weights, not separately additive GPU memory; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/cleanup.json |
| workspace-peak / coding.metal/turn-0 | NOT MEASURED | byte | Typed workspace peak when available; unprojected ownership stays unavailable, not zero; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/cleanup.json |
| artifact-mapping-resident / coding.metal/turn-0 | 9.50502113e+10 | byte | OS smaps artifact-mapping RSS at the post-request boundary; not an additional allocation or independent device copy; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/memory-after.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Not independently qualified by this timing capture;  |
| checkpoint-reference | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |
| deployment-performance | CHARACTERIZED | Frozen Rust/native request path; not interactive terminal-render timing or HTTP;  |
| family-conformance | UNQUALIFIED | Not independently qualified by this timing capture;  |
| product-path | CHARACTERIZED | Frozen Rust/native request path; not interactive terminal-render timing or HTTP;  |
| representation-quality | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |

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
| source_commit | 299598ae0370efbad588bd0eec245a29b294fd6f |
| source_tree | dd5877160821adfa6042bf38018c3b900b54b610 |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | d9555d3c5364527c2e70e1d18f78a6652133fa123b6c8b13673990a044c68932 |
| executable | 1e275c1f3eac90f0ad88332eeb5403a10808fc9a63a20e40f97d48f35be1227b |
| backend | cuda |
| backend_implementation | backend.cuda@299598ae0370efbad588bd0eec245a29b294fd6f+e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| kernel_bundle | 7d8704f5c943b735ddf0001b35d74357e4a629aabce948cf8fe48d5753477240 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | f636288a4cad8109b616785ffdb18d4c9c4b6710a6b2ab5c456a3b3fc33401d9 |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":1.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-prefill-policy-baseline-coding-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Existing native product defaults are greedy (stochastic=0, temperature=1), not stochastic HTTP defaults.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Unreached reasoning/final transitions remain NOT MEASURED; bounded output does not imply natural EOS.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- Changed configured chunk is an explicit product-policy experiment, not a kernel or HTTP speedup. Native bundle identities differ between independently compiled builds and remain separately authenticated.
- Temperature/power/clock variation is not continuously observed; boundary/resource witness is not a hardware reservation.
