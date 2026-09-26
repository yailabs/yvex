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

The original native computation remains one unpadded, token-domain `choice`
input shorter than the 128-token sliding-attention window (at most 64 admitted
tokens). Its direct caller supplies exact token IDs, type ID, opaque candidate
IDs and marker positions. The separate local producer now accepts bounded
question/context text and candidate ID/text pairs. Inside YVEX, the
source-owned tokenizer/family input policy is selected only for the exact
checkpoint source identity and verifies the tokenizer JSON and
tokenizer-configuration SHA-256 identities. It applies
the qualified `choice question:` and option/marker construction, and refuses
inputs needing truncation. The producer does not silently add chat formatting,
padding, calibration temperature or an action head. The direct token-domain
API retains its original meaning.

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
empty persistent host. `<yvex/server_finite_decision.h>` exposes direct
token-domain execution; `<yvex/finite_decision_producer.h>` exposes the
process-safe, generation-bound local producer on private protocol v24. A client
need not know Laya tokenizer IDs, marker positions or templates. Neither path
is an OpenAI-compatible route or a YAI semantic contract. Old protocol versions
refuse at negotiation. The CPU engine owns mapped source bytes and temporary
F32 execution workspace; `parameter_execution_bytes` is a logical F32
parameter extent, not an additional resident allocation. CUDA execution is
currently unsupported and fails closed.

The bounded producer comparison uses the independent upstream tokenizer and
PyTorch model on `Select the best option.` / `A short state.` with candidates
`continue`, `stop`, `escalate`. Both sides construct the same 29 input tokens
and marker positions `[10,14,18]`. Upstream logits are
`[0.752040267, 0.347543657, -0.223986357]`; native logits are
`[0.752036333, 0.347547591, -0.223984987]`, maximum absolute difference
`3.934e-6` against the predeclared `1e-4` tolerance for F32 reduction order.
This qualifies numerical equivalence only for the stated input and raw
finite-decision result, not general model quality, upstream calibration,
action/escalation meaning, multilingual breadth, production latency or release
fitness. The roughly 37-second CPU forward is characterization, not a claim
that this checkpoint is a low-latency System-1 tier on this hardware.
