<!-- docs:metadata
title: "DeepSeek paired greedy coding \u2014 native v24"
id: yvex.evaluation.qualification.deepseek-paired-greedy-native
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-paired-greedy-native.json
-->

# DeepSeek paired greedy coding — native v24

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-paired-greedy-native.json)

Target identity: `fcecd9f3ec6ef65227c743760db1a8120f97342ebaa77847c18c32b925efccf8`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 1.42855 | s | 3 | 1.41682–1.44769 | 0.0117352 |
| request.client-complete / coding.metal/turn-0 | 34.4162 | s | 3 | 34.396–34.4173 | 0.00106688 |
| ttft.client-visible / coding.metal/turn-0 | 3.61225 | s | 3 | 3.58829–3.61521 | 0.00296067 |
| ttft.server / coding.metal/turn-0 | 2.17148 | s | 3 | 2.16455–2.18666 | 0.00693124 |
| final.first.server / coding.metal/turn-0 | 2.17148 | s | 3 | 2.16455–2.18666 | 0.00693124 |
| final.first.client / coding.metal/turn-0 | 3.61225 | s | 3 | 3.58829–3.61521 | 0.00296067 |
| final.phase-rate / coding.metal/turn-0 | 8.17186 | token/s | 3 | 8.17128–8.17217 | 0.000310868 |
| decode.post-first.committed / coding.metal/turn-0 | 8.27862 | token/s | 3 | 8.2776–8.27909 | 0.000468726 |
| prefill.uncached / coding.metal/turn-0 | 32.1587 | token/s | 3 | 31.9278–32.3008 | 0.142122 |
| prefill.wall / coding.metal/turn-0 | 1.64808 | s | 3 | 1.64083–1.65999 | 0.00725147 |

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
| Draft phase | 2.49113 | s | 3 | 2.48467–2.49917 | 0.00646334 |
| Verification phase | 19.5208 | s | 3 | 19.5085–19.5337 | 0.0122967 |
| Speculative commit phase | 8.77163 | s | 3 | 8.76209–8.77582 | 0.00418818 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| checkpoint-reference | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| deployment-performance | CHARACTERIZED | Three bounded coding samples, exact binary and isolated resident host; 53 new input and 256 committed output tokens; no profiler;  |
| family-conformance | UNQUALIFIED | Local native measurement only; independent reference and producer provenance incomplete;  |
| product-path | CHARACTERIZED | Three bounded coding samples, exact binary and isolated resident host; 53 new input and 256 committed output tokens; no profiler;  |
| representation-quality | BLOCKED | Independent same-quantized-weight captures exist, but no admitted numerical comparison or higher-precision quality gate has passed; Complete distributions and checkpoint-matched numerical tolerance are not qualified; visible continuation differs from the independent F32-KV reference |
| backend-execution | UNQUALIFIED | Completed samples do not establish backend numerical/lifecycle qualification;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| checkpoint | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| tokenizer_conversation | NOT RETAINED |
| transformation_ir | NOT RETAINED |
| physical_policy | NOT RETAINED |
| representation | deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1@b669d807 |
| artifact_set | b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f |
| binding | 8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e |
| specialization | 3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144 |
| source_commit | d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9 |
| source_tree | 2e195df1c5d21b4ba0c2de4f1dbc0537a7fe35ca |
| source_delta | NOT RETAINED |
| build | 714466a812e75f4529e59a3e15975b7da0b770928f84892763583c95dfe50524 |
| executable | db15cca8f28f9c3f071d7e272b338214f52e5af8d7dc4b76788802f0e5480404 |
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
| sampling | explicit greedy; temperature=0 top_p=1; no penalties; no seed; native stochastic=0, HTTP temperature=0 |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **final.first.server**: server turn start to first source-classified final token. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **final.first.client**: client dispatch including connect to first nonempty final fragment. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **final.phase-rate**: source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: local client and server terminal-summary measurements; no quality qualification. Session: fresh; warm/cold: resident engine; no hidden warmup; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/paired-greedy-candidate-02/case-0/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- This is a bounded candidate characterization, not the operator's historical chunk=64 configuration, a release benchmark or the 20/700 Task exit.
- Same model, strategy, context, chunk, input, output bound and explicit greedy sampling; all six visible outputs match (923 bytes), with 136 accepted / 300 proposed tokens per sample.
- 53 input tokens do not establish uncached 2K/8K prefill. None-only coding does not qualify high/maximum or universal sustained decode.
- Native receipt excludes terminal rendering. HTTP/native client latency differences include setup/publication/teardown and run-order effects, not just transport.
- First committed-fragment publication timestamp is unavailable; server TTFT and client-visible TTFT remain different metrics.
- No KL, PPL, NLL or semantic equivalence is inferred from visible-output agreement. Independent reference uses the same quantized weights with explicit F32 KV.
- Missing target identities prevent automatic comparable-target ranking. Built source and measurement-harness source are retained separately.
