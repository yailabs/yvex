<!-- docs:metadata
title: Qwen3.8-27B Text Record
id: yvex.model-families.qwen3.8-text
document: reference
status: mixed
owner: model
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Qwen3.8-27B Text Record

**Exact BF16 hybrid execution and finite-candidate readout, with text-only scope.**

[Up](README.md)

## Source and compiled representation

Qwen's current text target is `Qwen/Qwen3.8-27B`, revision
`1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0`, interpreted by the
[`qwen3_5` model owner](../../src/model/families/qwen3_5.c) and
[graph recipe](../../src/graph/families/qwen3_5.c). The 64 text layers compose
48 recurrent sequence mixers and 16 full-attention layers. Text roles consume
851 of 1,199 source tensors; 333 vision and 15 MTP tensors do not enter the
admitted text artifact. Source presence does not publish input capability.
[Adapter tests](../../tests/unit/qwen_adapter.c) and
[architecture tests](../../tests/unit/qwen3_5_architecture.c) guard that boundary.

The unchanged 53,815,809,152-byte BF16 GGUF artifact
`1fce07008eaa78e04eedd1a031144f48eb6af617f2b5c508811ba91dca7e00f1`
is associated with immutable published release
`yailabs/Qwen3.8-27B-Text-GGUF@066eb288bffd5a07c0d5ca584114a1f3fcfd13a8`
and a fresh authenticated binding v17. Ordinary target-only CUDA execution
consumes the 1,732-step compiler-owned forward and two-step output programs,
produces 248,320 finite logits, and captures/attaches common prefix schema v2
with all 16 attention and 48 recurrent layers represented. Malformed retained
v16 canonical records remain refused. The ordinary admission control preceded the separate readout breadth qualification below. It does not establish upstream whole-model conformance.

## Readout and qualification

The completed Qwen breadth Task uses the same common readout schema and score
owner as the Mamba control. One captured prefix includes attention and recurrent
state; isolated candidate sessions teacher-force exact tokenizer-bound tokens.
Sampling/generated counts are zero and one backbone remains resident.

Independent long-double likelihood arithmetic, full-prefix replay, candidate
order, cancellation/retry and unchanged source-state controls qualify this exact
scope. [Retained observations](../evaluation/retained-observations.md#qwen-profile-and-hybrid-readout)
own the numerical observations; [allocation projections](../evaluation/benchmarks/generated/qwen-readout-state.md)
retain bounded state measurements. These are not calibrated confidence,
upstream whole-model conformance or a performance advantage.

## Shared owners and limits

Compiler context owns the nested physical program. Common runtime owners derive
and re-admit profile identity, capacity, hybrid state, resource projection and
cleanup. Qwen does not gain a private scoring runtime, prefix type or scheduler.

Only the exact admitted text representation is covered. Vision/MTP tensors in
upstream source do not confer input/output capability. Broader models, public
readout APIs, calibrated results, behavior evaluation and release qualification
remain separate obligations.
