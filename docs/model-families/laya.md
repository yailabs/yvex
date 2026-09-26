# Laya typed-decisions: first finite-decision model pressure

`SYSTEM.MODEL.LAYA.0` uses the immutable
[`convaiinnovations/laya-typed-decisions`](https://huggingface.co/convaiinnovations/laya-typed-decisions)
revision `1a793eb568e6718f15941d08f85432581df534e3` and the
[`NandhaKishorM/laya`](https://github.com/NandhaKishorM/laya) reference
implementation at `4066d5d5fbf08b66c6757ddeedbd797bd7655bc0`.
The selected 421M-parameter English ModernBERT-large typed checkpoint is a
stronger first finite-frontier pressure than the general English checkpoint;
the multilingual 322M checkpoint remains separate breadth. The upstream
checkpoint advertises application benchmarks and calibration-related fields;
YVEX does not inherit those claims.

The exact first scope is one unpadded, token-domain `choice` input shorter than
the 128-token sliding-attention window (at most 64 admitted tokens). The caller
supplies exact tokenizer-domain token IDs, question type ID, opaque candidate
IDs and their output-row positions. The binding authenticates the tokenizer
JSON SHA-256 and input policy; the current engine does **not** tokenize text or
construct Laya's text sequence. In particular, it does not silently apply
chat formatting, padding, truncation, calibration temperatures or an action
head. A caller needing text-to-frontier construction still needs a separately
qualified tokenizer/input producer.

The family interprets immutable source sidecars and compiles encoder and
decision-head parameter roles into common physical SSA. A common tensor
binding seals source, logical model, tokenizer, physical program and exact
parameter names. The artifact owner verifies the 842,609,220-byte F16
Safetensors source and 206-tensor inventory; the runtime stages F32 CPU
operations through the admitted backend. The generic finite-decision runner
selects output rows, returns raw logits and a numerically stable uncalibrated
softmax relative to the disclosed finite candidate population. It performs
one non-autoregressive forward, zero sampling and zero generated tokens.

An independently resident engine generation can be loaded into an otherwise
empty persistent host. The local C producer is
`<yvex/server_finite_decision.h>`; it is not an OpenAI-compatible route or a
YAI semantic contract. Local protocol v23 carries the new engine kind and
lifecycle facts, but not candidate-scoring requests. Old protocol versions
refuse at negotiation. The CPU engine owns mapped source bytes and temporary
F32 execution workspace; `parameter_execution_bytes` is a logical F32
parameter extent, not an additional resident allocation. CUDA execution is
currently unsupported and fails closed.

The bounded upstream comparison uses the independent PyTorch model's raw
candidate logits at exact marker positions `[10,14,18]` for one fixed token
sequence. Its tolerance is `1e-4` absolute for F32 reduction-order differences.
This qualifies numerical equivalence only for the stated input and raw
finite-decision result, not general model quality, upstream calibration,
action/escalation meaning, multilingual breadth, production latency or release
fitness. The roughly 37-second CPU forward is characterization, not a claim
that this checkpoint is a low-latency System-1 tier on this hardware.
