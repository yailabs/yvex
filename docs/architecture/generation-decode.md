<!-- docs:metadata
title: Generation and Decoding
id: yvex.architecture.generation-decode
document: architecture-plane
status: mixed
owner: runtime
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: generation-decode
related: [yvex.architecture.computational-state, yvex.architecture.advanced-generation]
-->

# Generation and Decoding

**Generate tokens through compiled model work and transactional publication.**

[Up](README.md)

Input rendering and exact tokenization precede admission. Prefill consumes the
accepted prompt, decode advances the model, logits feed a selected sampling
policy, and only committed state produces a published result.

## Ordinary generation loop

Prompt → tokenizer IDs → capacity/prefix admission → suffix prefill → model
step → logits → selection → state/token/RNG/decoder commit → channel fragment.

The tokenizer owns source-authored conversation grammar and channels. HTTP and
terminal adapters project typed results; they do not infer model grammar from
prose. [Speculation and finite results](advanced-generation.md) reuse this
substrate where their distinct semantics apply.

Families compile a bounded conversation grammar from the exact checkpoint's
authenticated template and tokenizer assets. Common role-envelope rendering
preserves an explicitly empty system message, requires a real user query and
uses source-declared tool-result delimiters when locating that query. Prior
reasoning removal and the generation prefix follow compiled policy. The runtime
does not execute arbitrary Jinja or let a frontend supply model role markers.

## Generation vocabulary

Prefill, ordinary decode, DSpark draft, target verification, and correction are
phase-specific work over one model schedule. They are not parallel model
implementations.

Configured prefill chunk size is not proof of physical parallelism. Prompt
execution resolves its phase-specific implementation width from the opened
engine, clamps real suffix batches to that admitted width, and preserves exact
position/state associations. Decode, draft, verification and correction retain
their own source-admitted populations. The
[specialization owner](deployment-specialization.md) seals those separate width
policies; capacity planning accounts for the corresponding larger work arena.
Wider prefill can reduce repeated traversal and synchronization while increasing
session preparation and peak memory. Compare complete-request measurements,
not only the inner prefill span.

```text
rendered prompt -> exact tokenizer IDs -> prefix admission -> suffix prefill
  -> target/draft/verify work -> normalized hidden -> output head -> selection
  -> state and decoder transaction -> committed channel fragment
```

Target-only remains the semantic reference. DSpark consumes source-authored
feature taps, proposes a bounded block, and asks the complete target to verify
it. Greedy DSpark commits the same target sequence as target-only from the same
state. Source-authored reasoning and final channels remain tokenizer-owned;
neither runtime nor backend infers a channel from prose.

Production CUDA may retain device values through Transformer, output head,
greedy/stochastic selection, and admitted speculative acceptance/correction.
The common sampling owner supplies transactional RNG, validates bounded result
publication, and commits RNG only with the surrounding state transaction.
Tokenizer and protocol remain host-owned; this is not all-on-device generation.
Transient hidden/logit publications have one producer-owned borrow generation;
workspace reuse expires old views before downstream selection. This replaces
passive producer counters with checked lifetime authority, without copying full
rows or adding a persisted identity. The [runtime contract](../contracts/runtime.md#cancellation-and-draining)
defines serialization and retirement limits.
Audit and forensic profiles may request bounded host evidence
or full reference intermediates. Those adapters are explicit and are not
reachable as a silent production fallback.


## Implementation and evidence

[src/runtime/generation.c](../../src/runtime/generation.c) · [include/yvex/internal/generation.h](../../include/yvex/internal/generation.h) · [include/yvex/internal/sampling.h](../../include/yvex/internal/sampling.h) · [src/tokenizer](../../src/tokenizer)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Computational State](computational-state.md) · [Speculation and Finite Execution](advanced-generation.md)
