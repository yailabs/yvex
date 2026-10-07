<!-- docs:metadata
title: "DeepSeek native candidate characterization"
id: yvex.evaluation.qualification.deepseek-native-throughput3-candidate
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-native-throughput3-candidate.json
-->

# DeepSeek native candidate characterization

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-native-throughput3-candidate.json)

Target identity: `282cfbf568f81a6774a7e8651a19ff477920922683a5dd49acf89c8b73269dd1`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| admission.client / coding.metal/turn-0 | 1.45647 | s | 3 | 1.3989–1.56801 | 0.0575734 |
| prefill.wall / coding.metal/turn-0 | 1.63537 | s | 3 | 1.62676–1.67506 | 0.00861141 |
| request.client-complete / coding.metal/turn-0 | 34.6907 | s | 3 | 34.5165–34.9541 | 0.174159 |
| ttft.server / coding.metal/turn-0 | 2.23323 | s | 3 | 2.16375–2.37224 | 0.069484 |
| ttft.client-visible / coding.metal/turn-0 | 3.63213 | s | 3 | 3.62023–3.94024 | 0.0119076 |
| prefill.uncached / coding.metal/turn-0 | 32.4086 | token/s | 3 | 31.6408–32.5802 | 0.171559 |
| decode.post-first.committed / coding.metal/turn-0 | 8.2226 | token/s | 3 | 8.21075–8.25402 | 0.0118506 |
| admission.client / conversation.coding/turn-0 | 1.40116 | s | 3 | 1.38726–1.43701 | 0.0139025 |
| prefill.wall / conversation.coding/turn-0 | 0.611306 | s | 3 | 0.608976–0.627346 | 0.00232953 |
| request.client-complete / conversation.coding/turn-0 | 2.90211 | s | 3 | 2.89145–2.93197 | 0.0106599 |
| ttft.server / conversation.coding/turn-0 | 1.12202 | s | 3 | 1.11688–1.13842 | 0.00513484 |
| ttft.client-visible / conversation.coding/turn-0 | 2.52568 | s | 3 | 2.51807–2.55905 | 0.00761094 |
| prefill.uncached / conversation.coding/turn-0 | 9.81506 | token/s | 3 | 9.5641–9.8526 | 0.0375458 |
| decode.post-first.committed / conversation.coding/turn-0 | 24.1274 | token/s | 3 | 23.9238–24.1556 | 0.0281938 |
| admission.client / conversation.coding/turn-1 | 0.00142536 | s | 3 | 0.000604801–0.00160464 | 0.00017928 |
| prefill.wall / conversation.coding/turn-1 | 0.608471 | s | 3 | 0.605942–0.611984 | 0.00252858 |
| request.client-complete / conversation.coding/turn-1 | 32.9377 | s | 3 | 32.7779–32.9719 | 0.0341821 |
| ttft.server / conversation.coding/turn-1 | 1.09808 | s | 3 | 1.09627–1.1025 | 0.00181484 |
| ttft.client-visible / conversation.coding/turn-1 | 1.09954 | s | 3 | 1.09725–1.10422 | 0.00229423 |
| prefill.uncached / conversation.coding/turn-1 | 14.7912 | token/s | 3 | 14.7063–14.8529 | 0.0617233 |
| decode.post-first.committed / conversation.coding/turn-1 | 8.0091 | token/s | 3 | 8.0023–8.05026 | 0.00680178 |
| admission.client / conversation.coding/turn-2 | 0.00139967 | s | 3 | 0.00103479–0.00169122 | 0.000291553 |
| prefill.wall / conversation.coding/turn-2 | 0.90992 | s | 3 | 0.905161–0.93252 | 0.00475866 |
| request.client-complete / conversation.coding/turn-2 | 35.4732 | s | 3 | 35.4425–35.97 | 0.0306949 |
| ttft.server / conversation.coding/turn-2 | 1.48431 | s | 3 | 1.47846–1.50428 | 0.00585064 |
| ttft.client-visible / conversation.coding/turn-2 | 1.48587 | s | 3 | 1.47966–1.50612 | 0.00621469 |
| prefill.uncached / conversation.coding/turn-2 | 29.673 | token/s | 3 | 28.9538–29.829 | 0.155998 |
| decode.post-first.committed / conversation.coding/turn-2 | 7.50335 | token/s | 3 | 7.39958–7.50875 | 0.00540182 |

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
| Draft phase | 2.51133 | s | 3 | 2.50172–2.70338 | 0.00961189 |
| Verification phase | 19.6504 | s | 3 | 19.4993–19.7398 | 0.0894666 |
| Speculative commit phase | 8.87586 | s | 3 | 8.75002–8.90192 | 0.026056 |
### conversation.coding/turn-0

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 6 | token | 3 | 6–6 | 0 |
| Reused prefix | 0 | token | 3 | 0–0 | 0 |
| New prefill | 6 | token | 3 | 6–6 | 0 |
| Committed output | 10 | token | 3 | 10–10 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 10 | token | 3 | 10–10 | 0 |
| Draft cycles | 2 | cycle | 3 | 2–2 | 0 |
| Draft forwards | 2 | forward | 3 | 2–2 | 0 |
| Proposed | 10 | token | 3 | 10–10 | 0 |
| Selected verification | 10 | token | 3 | 10–10 | 0 |
| Target verifications | 2 | verification | 3 | 2–2 | 0 |
| Accepted draft | 7 | token | 3 | 7–7 | 0 |
| Rejected draft | 3 | token | 3 | 3–3 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 2 | token | 3 | 2–2 | 0 |
| Per-sample mean accepted prefix | 3.5 | token | 3 | 3.5–3.5 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 0.0764956 | s | 3 | 0.0750845–0.0765224 | 2.6851e-05 |
| Verification phase | 0.627502 | s | 3 | 0.627369–0.633512 | 0.000132421 |
| Speculative commit phase | 0.161411 | s | 3 | 0.160531–0.162098 | 0.000687201 |
### conversation.coding/turn-1

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 25 | token | 3 | 25–25 | 0 |
| Reused prefix | 16 | token | 3 | 16–16 | 0 |
| New prefill | 9 | token | 3 | 9–9 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 61 | cycle | 3 | 61–61 | 0 |
| Draft forwards | 61 | forward | 3 | 61–61 | 0 |
| Proposed | 305 | token | 3 | 305–305 | 0 |
| Selected verification | 301 | token | 3 | 301–301 | 0 |
| Target verifications | 61 | verification | 3 | 61–61 | 0 |
| Accepted draft | 134 | token | 3 | 134–134 | 0 |
| Rejected draft | 167 | token | 3 | 167–167 | 0 |
| Discarded draft | 4 | token | 3 | 4–4 | 0 |
| Correction/bonus | 61 | token | 3 | 61–61 | 0 |
| Per-sample mean accepted prefix | 2.19672 | token | 3 | 2.19672–2.19672 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.61387 | s | 3 | 2.60875–2.63666 | 0.00511965 |
| Verification phase | 19.8928 | s | 3 | 19.8332–19.9125 | 0.0196394 |
| Speculative commit phase | 9.22064 | s | 3 | 9.15932–9.26439 | 0.0437467 |
### conversation.coding/turn-2

| Fact | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| Rendered prompt | 308 | token | 3 | 308–308 | 0 |
| Reused prefix | 281 | token | 3 | 281–281 | 0 |
| New prefill | 27 | token | 3 | 27–27 | 0 |
| Committed output | 256 | token | 3 | 256–256 | 0 |
| Reasoning | 0 | token | 3 | 0–0 | 0 |
| Final content | 256 | token | 3 | 256–256 | 0 |
| Draft cycles | 57 | cycle | 3 | 57–57 | 0 |
| Draft forwards | 57 | forward | 3 | 57–57 | 0 |
| Proposed | 285 | token | 3 | 285–285 | 0 |
| Selected verification | 285 | token | 3 | 285–285 | 0 |
| Target verifications | 57 | verification | 3 | 57–57 | 0 |
| Accepted draft | 142 | token | 3 | 142–142 | 0 |
| Rejected draft | 143 | token | 3 | 143–143 | 0 |
| Discarded draft | 0 | token | 3 | 0–0 | 0 |
| Correction/bonus | 57 | token | 3 | 57–57 | 0 |
| Per-sample mean accepted prefix | 2.49123 | token | 3 | 2.49123–2.49123 | 0 |
| Per-sample maximum accepted prefix | 5 | token | 3 | 5–5 | 0 |
| Draft phase | 2.80098 | s | 3 | 2.79644–2.81841 | 0.00454098 |
| Verification phase | 20.1856 | s | 3 | 20.1667–20.4191 | 0.0188963 |
| Speculative commit phase | 10.9829 | s | 3 | 10.9726–11.2025 | 0.0103168 |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | No inherited qualification;  |
| checkpoint-reference | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| representation-quality | BLOCKED | No independent inference/quality promotion; Exact-checkpoint independent full-model continuation and representation-quality comparison not yet obtained |
| backend-execution | UNQUALIFIED | Backend qualification is not inferred from a completed performance sample;  |
| deployment-performance | CHARACTERIZED | Three repetitions per representative native case; isolated host, current candidate binary; chunk differs from operator baseline;  |
| product-path | CHARACTERIZED | Three repetitions per representative native case; isolated host, current candidate binary; chunk differs from operator baseline;  |

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
| sampling | deterministic product defaults; stochastic=0 temperature=1 top_p=1 typical_p=1 no explicit seed |
| product_path | product-native-v24 |
| suite | 3f68f1a82695656aa1acbbc5a4e1668586f5b145132cbf9e63b071c2cf0f3a96 |

## Definitions and reproducibility

- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-0/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: fresh; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **admission.client**: client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.wall**: server-authored complete newly executed prefill wall time. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.server**: turn start to first committed model-token callback. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **ttft.client-visible**: client dispatch including connect to first nonempty final/reasoning content. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **prefill.uncached**: newly committed uncached input positions / complete prefill wall. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.
- **decode.post-first.committed**: committed tokens after first / elapsed decode wall after first. Scope: Typed native client, not chat terminal rendering; server phases retain their own clock origins. Session: reused; warm/cold: resident engine; first request retained, no hidden primer; output bound: 256. Evidence: /home/dgmothx/lab/models/evidence/yvex-measurement-authority-20261004.YbCrRR/native-candidate-03/case-1/events.jsonl.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Not a throughput Task closure, release benchmark, model-quality claim or chat-PTY performance claim.
- Same admitted artifact and checkpoint, but operator chunk=64 and candidate chunk=512 are distinct targets; source/build also differ.
- Old executable dirty source delta is unavailable. No source-clean causal speedup claim is inferred.
- A ten-token greeting is not sustained decode; 256-token coding is bounded decode evidence, not universal throughput.
- First publication and reasoning-to-final transition not measured; this target is reasoning none only.
- Independent output reference remains missing. Different generated token identities cannot be promoted to semantic equivalence.
- No uncached 2K/8K prefill, high/maximum reasoning, target-only or long-context qualification is imported.
