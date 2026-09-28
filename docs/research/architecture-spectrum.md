<!-- docs:metadata
title: Architecture Spectrum Research
id: yvex.research.architecture-spectrum
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Architecture Spectrum Research

**Pressure shared owners with different computational shapes.**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

Pressure shared owners with different computational shapes.

A spectrum target tests architectural assumptions. Only selected Tasks authorize execution; the spectrum is not a supported-model catalog.


## Architectural breadth strategy

Once expressible by common model language, typed state, component graphs and
runtime mechanisms, another model of that architecture should primarily need
source/compiler integration and physical qualification. Conventional models
are useful falsification tests too:

| Reference class | Architectural purpose |
| --- | --- |
| Llama-class dense decoder | Prove ordinary dense Transformer integration is inexpensive |
| Mixtral-class MoE | Separate common sparse execution from DeepSeek-specific mathematics |
| BERT/ModernBERT-class encoder | Break model-equals-causal-generator assumptions |
| T5-class encoder-decoder | Retained encoder state and cross-attention |
| Whisper-class speech encoder-decoder | Encoder-decoder lifecycle plus non-text input |
| Clean VLM | Reusable tower → connector → LM composition |
| Mamba2 | Reject Transformer-shaped state/decoder assumptions |

These are design probes, not additional scheduled milestones. Spectrum and
program evidence determine whether/when to use them. Existing DeepSeek,
Qwen and MiniMax evidence stays at its [family-specific scope][families].


[families]: ../model-families/integration.md

## A11 DeepSeek V4.1 Flash target pressure

**PLANNED; not acquired or implemented in YVEX.** The official target is
[`deepseek-ai/DeepSeek-V4.1-Flash`](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash).
The following public-source facts were inspected for roadmap research on
2026-09-13. They are structural targets, not YVEX support, immutable acquisition
evidence or measured YVEX resource use. No upstream revision is pinned here;
actual acquisition must later capture an immutable revision through the normal
source workflow.

| Upstream structural fact | Reference value |
| --- | --- |
| Model type | `deepseek_v41` (nested text configuration: `deepseek_v41_text`) |
| Parameter populations | 552B backbone; approximately 196B conditional Engram parameters |
| Language stack | 40 layers: causal encoder 20 → decoder 20; hidden width 5120 |
| Active backbone | Approximately 8B/token prefill; 16B/token decode |
| MoE | 384 routed experts; top-6; one shared expert |
| Context | Up to 1M tokens; configured maximum 1,048,576 |
| Global KV | Approximately 890 bytes/token; main KV FP4/E2M1 with one E4M3 scale per 16 channels |
| Engram sites / rows | Layers 1 and 14; respectively 384,006,168 and 384,016,682 rows |
| Engram addressing | Maximum n-gram size 4; 8 heads; head width 256; compressed vocabulary 99,092 |
| CSA2 KV sources | Layers 2, 8, 14, 20 |
| CSA2 index sources | Layers 2, 8, 14, 20, 24, 28, 32, 36 |
| Index geometry | 32 heads × 128; top-k 512 |
| Hierarchical candidates | Source layer 20; 2048 candidate blocks × 8 |
| Hyper-connections | Multiplicity 4; revised Single-Pass mHC / Mega-mHC |
| DSpark | 3 next-token layers; block size 5; target taps 37, 38, 39 |

Architecture totals, phase activation, FP4 scale granularity and mechanisms come
from the [official model card](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/README.md).
Exact dimensions, layer indices and addressing cardinalities come from the
[official configuration](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/config.json)
and [inference configuration](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/inference/config.json).
The [official Engram reference](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/inference/engram.py)
also exposes compressed-token mapping, window/hash partitions, padding and
image-boundary handling: these must become exact source semantics, not runtime
string heuristics. Public-source inspection does not admit that code into YVEX.

V4.1 is a distinct architecture pressure. [Current V4/DSpark][deepseek] retains
its 43-layer SWA/compressed sparse/heavy-compression hybrid, existing mHC,
256 routed/top-6 experts and current target/draft relationship. Its evidence and
v0.1 target remain unchanged. V4.1 instead adds CED, CSA2 cross-layer sharing,
FP4 KV, Engram, revised mHC, 384 experts, a different DSpark topology and a vision
source architecture. A successor name or matching shape establishes no support.

The CED source shape is causal encoder → terminal encoder hidden state → global
decoder KV projection → autoregressive decoder. CSA2 assigns static Full,
Reindex and Reuse modes with shared main KV, indexer keys and reused Top-K
indices; the hierarchical indexer uses a candidate pool from the first Full
decoder layer. These are source-declared dependencies to project through common
program/state semantics, not per-layer duplicate KV owners or a CSA2 runtime.
They pressure [phase liveness](sparse-parameter-memory.md#phase-liveness-and-quantized-runtime-state), beyond
ordinary encoder-decoder recognition.

Single-Pass mHC requires separate source interpretation, typed semantics,
independent reference numerics, physical lowering and backend numerics. Current
V4 mHC evidence proves only its admitted scope. Likewise broader MoE routing
does not imply a new runtime: total parameters != active parameters != resident
parameters != moved parameters. Learned expert routing and token-addressed
Engram select different populations with distinct semantics/accounting, even if
their physical caches eventually share mechanisms.


[deepseek]: ../model-families/deepseek-v4-flash.md

## Bounded GB10 text target and evidence ladder

The first intended machine pressure is one GB10 node in the approximately
128 GB unified-memory class: **text, target only, small admitted context, bounded
prompt, one generated token**. There is no YVEX fit claim before exact
source/physical inventory, resource admission and execution evidence.

The useful terminal boundary is exact official source → inventory → architecture
import → typed semantic/program projection → admitted physical recipe → GB10
load/resource admission → complete causal encoder on a bounded prompt →
encoder-to-decoder state projection → decoder → complete logits → selected token.
No partial layer/tensor result substitutes for that complete path.

| Explanatory evidence boundary | Required future result |
| --- | --- |
| V41-SOURCE | Official immutable acquisition and exact config/tokenizer/tensor inventory |
| V41-IMPORT | CED, CSA2, Engram, Single-Pass mHC, MoE and target relationships projected through common semantics |
| V41-PHYSICAL | GB10-oriented text/target-only recipe, conditional-table and runtime-state representations, explicit resource envelope |
| V41-REFERENCE | Bounded independent/component numerics, including addressing and refusal vectors |
| V41-CUDA | Admitted GB10 primitives with reference/backend numerical evidence |
| V41-RESIDENCY | Phase-aware parameter lifetime, expert residency/cache and conditional-table backing/cache; policy-equivalent results |
| V41-TEXT-ONE-TOKEN | Complete bounded source → encoder → decoder → logits → token execution |

Rows describe a non-scheduled dependency/evidence progression in that order,
not new ACTIVE/NEXT milestones. Later evidence may expand to multiple tokens,
continued prefill, larger contexts, DSpark, vision and provider breadth. None is
an initial terminal requirement. Neither 1M context, long generation, release
performance nor full provider parity belongs to the first target.

Refoundation .1 supplies the common compiler/runtime authority on which this
future integration depends; A11 adds no .1 closure criterion. Importers own
irreducible source meaning, while canonical compiler/runtime and admitted
backend owners execute it. No V4.1-specific runtime, application family branch,
YAI/Case dependency or B1 dependency is introduced. A11 must eventually execute
through ordinary YVEX model/provider semantics independently of Program N.


## External feasibility evidence

The [Dwarf Star conversion/execution record](https://huggingface.co/antirez/deepseek-v4.1-flash-gguf)
reports a Q2 file of 340.60 GiB: 151.77 GiB main weights plus 188.83 GiB Engram
tables. It documents one 128 GB Mac using SSD streaming and a two-128-GB-Mac
tensor-parallel path; Engram remains file-backed rather than fully resident.
The documented V4.1 execution path is Metal. These are producer-reported
**external physical-feasibility/execution evidence**, not runs reproduced by
YVEX or evidence for CUDA, GB10, a YVEX physical recipe, numerics, fit, performance
or release qualification. The byte figures exclude context/runtime reserves.
No per-rank residency figure is adopted here.

This motivates investigating bounded 128 GB-class execution with non-resident
parameter mechanisms. It neither overrides official model mathematics nor
selects Dwarf Star's implementation/cache architecture for YVEX. YVEX's target
is compiler-proven phase/resource liveness informing physical specialization;
no speed advantage over model-wide streaming is predicted.


## A12 Native finite-decision model pressure

A12 is the computational model class, not a YVEX `System Model` type. The latter
is a possible YAI consumer role. A12 tests whether a non-autoregressive admitted
model can consume a bounded typed question/context and caller-supplied dynamic
candidate frontier, then return identity-bound candidate scores without text
generation. This differs from A08's BGE-M3 encoder-only retrieval/vector-output
pressure and from token-likelihood Decision Readout over an existing generator.

`SYSTEM.MODEL.LAYA.0` and `FINITE.DECISION.PRODUCER.0` already earn A12's
PARTIAL state at one exact scope: `convaiinnovations/laya-typed-decisions`
revision `1a793eb568e6718f15941d08f85432581df534e3`, native compiler-owned
bidirectional CPU execution, authenticated binding/engine generation and a
separate local protocol-v24 client. Source-owned input construction hides
tokenizer, template, type and marker mechanics. The independent upstream
control matches 29 input tokens and candidate markers `[10,14,18]`; native
three-candidate raw logits differ by at most `3.934e-6` under the declared
`1e-4` tolerance. Zero sampling/generated tokens, engine replacement refusal
and load/unload lifecycle are qualified at that bounded scope. [The Laya
record](../model-families/laya.md) owns the detailed authority and evidence.

This does not establish general decision-model breadth, CUDA, useful low
latency, multilingual coverage, calibrated probabilities, action-head utility,
future fine-tuning, YAI integration or semantic authority. No A12 follow-up is
selected solely because these gaps exist. The Output Runner maturity row remains
PARTIAL and separate from the Spectrum evidence state.


## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
