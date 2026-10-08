<!-- docs:metadata
title: "GB10 Rust/native product coding.metal / high"
id: yvex.evaluation.qualification.deepseek-gb10-long-turn-native-coding-high-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-gb10-long-turn-native-coding-high-20261008.json
-->

# GB10 Rust/native product coding.metal / high

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-gb10-long-turn-native-coding-high-20261008.json)

Target identity: `c56e333fad497939fb54dcac42313adc33b1cbd4c0c9305e43e39da474b0c695`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 0.421756 | s | 3 | 0.409009–0.466858 | 0.0127467 |
| request.client-complete / coding.metal/turn-0 | 297.975 | s | 3 | 295.692–298.953 | 0.978821 |
| ttft.client-visible / coding.metal/turn-0 | 2.21585 | s | 3 | 2.09284–2.26913 | 0.0532795 |
| ttft.server / coding.metal/turn-0 | 1.79408 | s | 3 | 1.68383–1.80227 | 0.00819632 |
| reasoning.first.server / coding.metal/turn-0 | 1.79408 | s | 3 | 1.68383–1.80227 | 0.00819632 |
| reasoning.first.client / coding.metal/turn-0 | 2.21585 | s | 3 | 2.09284–2.26913 | 0.0532795 |
| final.first.server / coding.metal/turn-0 | 184.731 | s | 3 | 184.574–185.409 | 0.156672 |
| final.first.client / coding.metal/turn-0 | 185.198 | s | 3 | 184.996–185.818 | 0.201933 |
| reasoning.phase-rate / coding.metal/turn-0 | 8.22271 | token/s | 3 | 8.19341–8.2342 | 0.0114874 |
| final.phase-rate / coding.metal/turn-0 | 7.63988 | token/s | 3 | 7.62921–7.81151 | 0.0106724 |
| decode.post-first.committed / coding.metal/turn-0 | 8.02056 | token/s | 3 | 7.99076–8.08435 | 0.0298042 |
| prefill.uncached / coding.metal/turn-0 | 42.8702 | token/s | 3 | 40.3423–43.6778 | 0.807627 |
| prefill.wall / coding.metal/turn-0 | 1.23629 | s | 3 | 1.21343–1.31376 | 0.0228597 |

## Runtime populations and speculative work

Counters come from terminal runtime facts. Missing counters are not zero.
Channel totals may exclude control delimiters; phase spans are not assumed additive.

### coding.metal/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 53 | token | 3 | 53–53 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 53 | token | 3 | 53–53 | 0 |
| Committed output | 2373 | token | 3 | 2373–2373 | 0 |
| Reasoning | 1509 | token | 3 | 1509–1509 | 0 |
| Final content | 863 | token | 3 | 863–863 | 0 |
| Draft cycles | 345 | cycle | 3 | 345–345 | 0 |
| Draft forwards | 345 | forward | 3 | 345–345 | 0 |
| Proposed | 1725 | token | 3 | 1725–1725 | 0 |
| Selected verification | 1725 | token | 3 | 1725–1725 | 0 |
| Target verifications | 345 | verification | 3 | 345–345 | 0 |
| Accepted draft | 821 | token | 3 | 821–821 | 0 |
| Rejected draft | 904 | token | 3 | 904–904 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 345 | token | 3 | 345–345 | 0 |
| Per-sample mean accepted prefix | 2.37971 | token | 3 | 2.37971–2.37971 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 15.3999 | s | 3 | 15.1811–15.4058 | 0.00593973 |
| Verification phase | 102.119 | s | 3 | 101.982–102.188 | 0.0692167 |
| Speculative commit phase | 63.0252 | s | 3 | 62.6074–63.3069 | 0.281739 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Not independently qualified by this timing capture;  |
| checkpoint-reference | BLOCKED | Not independently qualified by this timing capture; No independent full-model checkpoint/quality comparison in this timing series |
| deployment-performance | CHARACTERIZED | Three long coding/high native turns with exact product geometry and source identity; characterization, not the 20/700 gate.;  |
| family-conformance | UNQUALIFIED | Not independently qualified by this timing capture;  |
| product-path | CHARACTERIZED | Three fresh native coding/high turns on the exact frozen target complete naturally at 2,373 committed tokens, with source-classified reasoning and final content; not terminal paint timing, HTTP, representation quality or universal session correctness.;  |
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
| source_commit | f1c47701528dc5d4f8a2ad5f8dd04daea3230939 |
| source_tree | d7f2ee106fdf85e1ddc137793c66cddb70b6b30b |
| source_delta | 0c278671297bb0bfa38e8e81b27eb69e045c5176d131f3d28e5c337d6384fc55 |
| build | cda34906ee5ae0fea6d861cfd222500689291023946ba5925ceef660bb46824e |
| executable | 82c1418e5c16b83d79202dec67b663041daddf7766f88a16770232f57fd4bbed |
| backend | cuda |
| backend_implementation | backend.cuda@f1c47701528dc5d4f8a2ad5f8dd04daea3230939+0c278671297bb0bfa38e8e81b27eb69e045c5176d131f3d28e5c337d6384fc55 |
| kernel_bundle | 297c53e26f9c14cb2c4ac9f53c13429b655f2cba19dc431620c55043b765e665 |
| hardware_model | NVIDIA GB10 |
| device_count | 1 |
| topology | single-node single-device coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA Driver API; toolkit 13.0 / NVCC V13.0.88 |
| memory_configuration | coherent unified memory; CUDA-reported global memory bytes=130663170048 |
| runtime_configuration | 7b84402b68f8da82a362d51bba99ea4618296a9767438193f941d1bae1084b1f |
| context | 32768 |
| prefill_geometry | chunk=64 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | speculative |
| reasoning | high |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":1.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | b048ce89750a4c93f9e6f667a101eca271c05295fef932724b8f4c7b8dec0176 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **reasoning.first.server**: server turn start to first source-classified reasoning token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **reasoning.first.client**: client dispatch including connect to first nonempty reasoning fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **reasoning.phase-rate**: source-classified reasoning tokens / server phase from prefill completion to reasoning boundary or decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 4096. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/product-long-coding-high-native-01/native/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- N=3 workload-bound characterization; not 20/700, independent quality or universal product speed.
- Native Rust request/committed-content observation, not REPLAI paint/editor timing or HTTP.
- Existing native product defaults are greedy (stochastic=0, temperature=1), not stochastic HTTP defaults.
- Exact engine configuration retained; first post-load request and subsequent fresh sessions are not cold samples.
- Sampled process witness cannot prove uninterrupted reservation. RSS/mapping/CUDA ownership overlap.
- First-fragment server publication remains unavailable; client-visible and internal clocks are separate.
- All three observed turns reached natural EOS; no missing independent quality evidence or unexecuted reasoning/strategy cell is qualified by that completion.
- Long-turn suite retains exact prompt bytes but changes the output bound from 256 to 4096; these lengths are separate experimental axes, not directly comparable throughput rows.
