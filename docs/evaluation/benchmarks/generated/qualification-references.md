<!-- docs:metadata
title: "Independent checkpoint reference captures"
id: yvex.evaluation.qualification.references
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../methodology.md
-->

# Independent checkpoint reference captures

[Benchmarks](../README.md) · [Methodology](../methodology.md)

Capture presence is not YVEX numerical agreement or representation-quality qualification.
Source-parser results below concern the independent producer. Missing quality metrics are not zero.

## deepseek-independent-history

[Machine observation](../references/deepseek-independent-history.json)

| Identity | Value |
| --- | --- |
| Model / checkpoint | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| Upstream revision | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| Independent producer | llama.cpp-deepseek-v4-flash @ 2f2d44052b7d15c9c4dd6610f6e14a5f7b2d5f3f |
| Executable | df2fee12b815701ebaa2eaaf0d840fdda26a98b83a2f14f47a2249d5ea254446 |
| Representation receipt identity | 2924ddfeafde680f907453fb48290b7cf14eb0ede5a2894560169acb4e698403 |
| Environment receipt identity | 95087fb2b561c96b234291be5fccd6e7a33194e92d6a7c8ae9ed5a9000d8cd21 |
| Reference capture | 04a24c560eb5663d5985ce54ce2f210bf11033db357371d857b26c66f6064b27 |
| Workload suite bytes | 499d84eef0005dc065838d53f41d9945aec654b51df94a11976db5505a429ad2 |
| Weight manifest | 754759502d1120afd9f82c953751c7fb40a5fb74e4f4b42c53720197a37894b7 |
| Capture / YVEX conformance | CHARACTERIZED / NOT_RUN |

| Case | Reasoning | Input | Output / bound | Finish | Source grammar | Reasoning boundary positions |
| --- | --- | ---: | ---: | --- | --- | --- |
| conversation.coding/turn-1 | none | 25 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |

### Independent generated-history continuation

| Case | Reasoning | Next turn | Prior grammar | Evidence state | Reason |
| --- | --- | ---: | --- | --- | --- |
| conversation.coding | none | 1 | PASS | CHARACTERIZED | source-encoded actual independent history |
| conversation.coding | high | 1 | FAIL | UNQUALIFIED | prior assistant did not complete source-authored grammar; no fabricated history |
| conversation.coding | maximum | 1 | NOT_MEASURED_TRUNCATED | UNQUALIFIED | prior assistant did not complete source-authored grammar; no fabricated history |

### Limits

- Inherited producer-plan note: Same-quantized-weight independent target-only execution; not the higher-precision upstream checkpoint.
- Inherited producer-plan note: Exact BF16 weight widening and F32 KV are declared reference differences, not YVEX representation changes.
- Inherited producer-plan note: Top-20 probabilities cannot establish full-vocabulary KL, RMS, NLL or PPL.
- Inherited producer-plan note: Multi-turn references capture their first input only; generated-history continuations remain missing.
- Inherited producer-plan note: Reference timings are not YVEX performance; capture does not qualify YVEX numerical agreement.
- Actual coverage here is the authenticated second request listed below, with independent generated history; not native prefix reuse or complete multi-turn qualification in all modes.

## deepseek-independent-target

[Machine observation](../references/deepseek-independent-target.json)

| Identity | Value |
| --- | --- |
| Model / checkpoint | deepseek-ai/DeepSeek-V4-Flash-DSpark |
| Upstream revision | 62af8fffb2f7030cac4de2f0169f5b8d1101b646 |
| Independent producer | llama.cpp-deepseek-v4-flash @ 2f2d44052b7d15c9c4dd6610f6e14a5f7b2d5f3f |
| Executable | df2fee12b815701ebaa2eaaf0d840fdda26a98b83a2f14f47a2249d5ea254446 |
| Representation receipt identity | 85a9a20bf65701eb7546fac8501a965fdfcd3130ba7080d639444f32d4d728a8 |
| Environment receipt identity | bf07510b4bea8d0eef5cd856e97395ff583f6b1eb480610594f39b01b22eebc5 |
| Reference capture | b28690aea05032e5aee4dceb55e57c648c638e9acd23f223533623efc20f1107 |
| Workload suite bytes | 499d84eef0005dc065838d53f41d9945aec654b51df94a11976db5505a429ad2 |
| Weight manifest | 754759502d1120afd9f82c953751c7fb40a5fb74e4f4b42c53720197a37894b7 |
| Capture / YVEX conformance | CHARACTERIZED / NOT_RUN |

| Case | Reasoning | Input | Output / bound | Finish | Source grammar | Reasoning boundary positions |
| --- | --- | ---: | ---: | --- | --- | --- |
| chat.short | none | 23 | 235 / 256 | eos | PASS | [] |
| chat.short | high | 23 | 254 / 256 | eos | PASS | [91] |
| chat.short | maximum | 102 | 155 / 256 | eos | PASS | [95] |
| coding.metal | none | 53 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| coding.metal | high | 53 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| coding.metal | maximum | 132 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| math.reasoning | none | 54 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| math.reasoning | high | 54 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| math.reasoning | maximum | 133 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| extraction.json | none | 60 | 91 / 256 | eos | PASS | [] |
| extraction.json | high | 60 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| extraction.json | maximum | 139 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| writing.runbook | none | 41 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| writing.runbook | high | 41 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| writing.runbook | maximum | 120 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| conversation.coding | none | 6 | 11 / 256 | eos | PASS | [] |
| conversation.coding | high | 6 | 11 / 256 | eos | FAIL | [] |
| conversation.coding | maximum | 85 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| reasoning.schedule | none | 40 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| reasoning.schedule | high | 40 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| reasoning.schedule | maximum | 119 | 256 / 256 | length | NOT_MEASURED_TRUNCATED | [] |
| tools.weather | none | 341 | 12 / 128 | eos | PASS | [] |
| tools.weather | high | 342 | 39 / 128 | eos | PASS | [26] |
| tools.weather | maximum | 420 | 40 / 128 | eos | PASS | [27] |

### Limits

- Same-quantized-weight independent target-only execution; not the higher-precision upstream checkpoint.
- Exact BF16 weight widening and F32 KV are declared reference differences, not YVEX representation changes.
- Top-20 probabilities cannot establish full-vocabulary KL, RMS, NLL or PPL.
- Multi-turn references capture their first input only; generated-history continuations remain missing.
- Reference timings are not YVEX performance; capture does not qualify YVEX numerical agreement.
