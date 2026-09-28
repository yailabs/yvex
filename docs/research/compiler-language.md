<!-- docs:metadata
title: Compiler Language Frontier
id: yvex.research.compiler-language
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Compiler Language Frontier

**Which model semantics can enter the shared typed language?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

Which model semantics can enter the shared typed language?

A new model shape must compile without a runtime family switch. Structural expressibility alone is insufficient: execute a bounded real consumer and compare independent numerical obligations.


## Semantic Model IR target language

This is the language the compiler must eventually express, not a claim that
every primitive, composition or source format below executes today.

| Semantic domain | Required representational breadth | Why it matters |
| --- | --- | --- |
| Embeddings | Token, positional, tied/untied output embedding | Dense language, encoder and output-head families |
| Normalization | RMSNorm, LayerNorm and required variants | Source-authored numerical authority |
| Dense FFN | GELU, SiLU, SwiGLU, GeGLU, gated MLP | Transformer breadth without mandatory FFN presence |
| Attention | MHA, MQA, GQA | Dense encoders and decoders |
| Attention policy | Causal, non-causal, sliding/local/global, cross-attention | Encoder state and long-context policies |
| Position | RoPE, partial/scaled RoPE, required absolute/relative forms | Position meaning sealed before execution |
| Latent attention | MLA-class projection and state | DeepSeek-class latent representations |
| MoE | Router, top-k, routed/shared experts, grouping | Sparse execution independent of one family |
| Recurrent | Typed recurrent state and transitions | RWKV and hybrids |
| SSM | Selective scan / Mamba-class state | Pure and hybrid state-space models |
| Convolution | Causal/stateful convolution | SSM, speech and hybrid models |
| Output heads | LM, embedding, pooling, classification, scoring | Execution beyond chat |
| Decision readout | Shared/current computation, finite candidate representations, score-producing readout, optional calibration or learned head, typed multi-result output and exact backbone/readout/head identities | Model-independent finite-option inference without making generation or semantic authority the execution contract |
| Multimodal composition | Tower, connector/projector, fusion | Image, audio and video component graphs |
| Iterative generation | Diffusion/flow/denoising state machine | Image/video and diffusion-language systems |
| Component composition | Target/draft, encoder/decoder, tower/LM, codec/decoder | One substrate for composite models |
| Persistent state input | Typed independently retained model-state inputs | Consume computational state beyond ordinary context |
| State Read | Explicit operation consuming persistent state | No hidden state-injection convention |
| State Update | Explicit operation producing state changes | Architecture-owned update semantics |
| State-producing blocks | Blocks contributing to persistent state | Express trained state architectures |
| State-consuming blocks | Blocks reading persistent state | Layer-aware state integration |
| State gating | Learned/declared interaction between primary computation and state | No universal injection mechanism |
| Cross-state attention / interaction | Optional interactions between computational state streams | Support distinct architectural realizations |
| Persistent banks | Typed model-native banks with explicit geometry | Separate representation from lifecycle |
| Multi-timescale state roles | Different computational update/retention scales | No hard-coded semantic-memory categories |
| State realization profile | Model-specific StateProfile, distinct from the B1-v0 reference choices | Bind exact representation, interaction, update and lifecycle capability |
| State-root effects and execution DAG | Explicit roots, independent branches, multi-result state transitions and bounded barriers | Preserve legal independence without prescribing CUDA concurrency |
| Trainable architecture augmentation | Parameter roles and R/E composition meaning | Describe a post-trainable model without owning optimizer policy |
| Token-derived addressing and conditional parameter reads | Exact window/key construction, indexed immutable reads, sparse row populations, combination/projection and gating | Bind table/tokenizer identity without a universal Engram primitive or whole-table dense operand |
| Phase-asymmetric program composition | Components with different computation, state dependencies and parameter/workspace liveness | Express causal encoder → state projection → decoder through common compilation |
| Shared cross-layer state | Explicit producer/source relationships for state and derived selections | Reuse a produced state or index population without duplicating ownership per layer |

The target model signature can grow from input → model → output to
`(primary input, persistent computational state)` → model graph →
`(output, updated persistent computational state)`. This is target-language
breadth, not a claim that current models expose that interface.


## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
