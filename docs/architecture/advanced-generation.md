<!-- docs:metadata
title: Speculation and Finite Execution
id: yvex.architecture.advanced-generation
document: architecture-plane
status: mixed
owner: runtime
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: advanced-generation
related: [yvex.architecture.generation-decode, yvex.architecture.interfaces-protocols]
-->

# Speculation and Finite Execution

**Shared runtime machinery supports distinct result semantics.**

[Up](README.md)

Speculation proposes tokens for a target to verify. Decision Readout scores
supplied token candidates. Native finite-decision execution computes a model's
typed finite frontier. These are different computational contracts.

| Path | Input and result | Publication authority |
| --- | --- | --- |
| Target-verified speculation | Draft candidates → accepted token prefix | Complete target verification + transaction |
| Decision Readout | Exact prefix/token candidates → likelihood scores | Identity-bound arithmetic over admitted logits |
| Native finite decision | Bounded text frontier → model-native raw scores | Exact input policy and admitted finite program |

## Speculative state

Target-only remains the semantic reference. Draft probabilities or confidence
cannot publish output. Greedy equality or the admitted stochastic accept/reject
policy chooses an exact target checkpoint; all state participants commit it
together. [ADR 0004](../decisions/0004-target-verified-speculation.md) owns the selection.

## Current finite boundaries

Decision Readout branches isolated sessions from one captured prefix, teacher
forces candidates, and computes raw likelihood without sampling or generating
tokens. Its exact Mamba CPU and Qwen hybrid CUDA controls do not imply universal
family support or calibrated probabilities.

The [Laya record](../model-families/laya.md) owns the separate CPU finite model
and local producer evidence. Opaque candidate meaning remains consumer-owned.
Neither scores nor a finite-population normalization confer YAI decision authority.

## Open frontier

Learned/calibrated readout, general scoring/embedding heads and R/E-aware
Decision Core are [target/research](../research/output-runners.md). A future
stateful readout must preserve the same compiler/runtime ownership.

## Implementation and evidence

[include/yvex/internal/decision_readout.h](../../include/yvex/internal/decision_readout.h) · [include/yvex/finite_decision.h](../../include/yvex/finite_decision.h) · [include/yvex/finite_decision_producer.h](../../include/yvex/finite_decision_producer.h) · [src/runtime/finite_decision.c](../../src/runtime/finite_decision.c)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Generation and Decoding](generation-decode.md) · [Interfaces and Protocols](interfaces-protocols.md)
