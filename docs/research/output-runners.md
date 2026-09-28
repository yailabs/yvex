<!-- docs:metadata
title: Output Runner Research
id: yvex.research.output-runners
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Output Runner Research

**Which useful model results do not require generating a conversation?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

Which useful model results do not require generating a conversation?

Compare exact score semantics and lifecycle before calibration or consumer usefulness. A relative distribution over a finite set is not calibrated confidence or decision authority.


## Decision Readout / Decision Core target

This target adopts a strict cross-project ownership boundary. The **Decision
Plane** is YAI's semantic layer: it owns Case/task meaning, qualified working
state, candidate meaning and origin, disclosure and authority context, semantic
interpretation and canonical admission. **Decision Readout** is YVEX's
computational finite-candidate primitive. **Decision Core** names a future more
general learned computational realization. A model/readout result grants no
authority, permission, effect or canonical Decision.

```text
current/shared model computation ─┐
                                  ├── computational readout ──► typed scores[N]
finite explicit candidates ───────┘                          + evidence
```

The preferred architecture is **one resident backbone → multiple qualified
computational readouts**. A future specialized decision model remains allowed,
but duplicating a full resident decision LLM is not part of the generic
contract. `DECISION_READOUT`, `SCORE`/`OPTION_SCORE`, `GENERATE`,
`EMBED`/`REPRESENT`, `STATE_READ` and `STATE_UPDATE` are target vocabulary, not
frozen public enums. Bare `DECIDE` is avoided because computation does not own
semantic authority.

| Research level | Target realization | Explicit boundary |
| --- | --- | --- |
| V0 | Already-loaded admitted backbone; shared/prefetched prefix or current computational state; finite candidates; direct option scoring | Zero generated tokens, no autoregressive sampling loop, no new trainable weights and no second full backbone. Known candidates may still require real teacher-forced forwards. |
| V1 | V0 plus a separately qualified small calibration transform such as temperature/bias | Relative scores are not called calibrated probability without independent evidence. |
| V2 | Frozen backbone plus a tiny learned linear probe/decision head over a qualified hidden representation | Head source, parameters, representation and backbone compatibility acquire exact identities. |
| V3 | General learned Decision Core over computational state and arbitrary finite candidates | Still computational and non-authoritative. |
| V4 | `DecisionCore(R, E, query, candidates)` | R/E-aware readout is a possible future read-only State Read consumer, not E or State Update. |
| V5 | Native System-One realization and/or dedicated specialized decision model | Requires independent usefulness, calibration and systems evidence; no current claim. |

These levels are an evidence/research ladder, not six scheduled milestones.
V0 selected Mamba-Codestral-7B-v0.1 only after live artifact archaeology showed
it was the smallest exact READY/launchable model with tokenizer, output logits
and common recurrent-prefix ownership. That qualified reference does not make
Decision Readout family-specific or schedule V1.

V0 explicitly does not implement calibration, a learned probe/head, a dedicated
decision model, R/E or StateProfile, a YAI contract, semantic Decision
admission, an adaptive cognitive router, System-One claims or a performance
advantage. It does not add a second full resident backbone.

Score meanings remain explicit and non-interchangeable:

| Quantity | Meaning / nonclaim |
| --- | --- |
| Raw logits | Unnormalized model output in an exact token/head domain. |
| Token log-probability | Normalized likelihood of a token under an exact conditioning history. |
| Candidate log-likelihood | Declared aggregation over the candidate's teacher-forced tokens. |
| Length-normalized candidate score | A separately declared normalization of candidate likelihood, not raw likelihood. |
| Relative candidate distribution | Normalization over the disclosed finite candidate population only. |
| Calibrated probability | Requires an explicit calibration method, artifact/identity where applicable and independent calibration evidence. |
| Confidence / uncertainty evidence | A separately defined qualified statistic; neither a softmax value nor semantic authority by default. |

In particular, `softmax(scores) != calibration`. A future System-One claim
cannot be inherited from ordinary LLM scoring or from an external producer's
claim.

The existing programs remain the owners:

| Program | Decision-readout responsibility | Boundary |
| --- | --- | --- |
| O — Output Runners | Primary owner of the typed finite-candidate request/result lifecycle and zero-decode runner semantics. | Does not own candidate meaning, application ranking policy or admission. |
| C — Model Language & Compiler | Shared/current representation, candidate representation, score-producing operations, optional calibration/head semantics, multi-result output and exact computational identities. | Does not freeze a YAI request type or family-specific runtime branch. |
| Q — Qualification | Score definition, state/prefix preservation, zero-generation proof, numerical/calibration evidence and latency/resource measurement. | Selection/calibration data cannot independently qualify the final result. |
| P — Physical Model Compiler | Future learned probe/head dtype/qtype, layout, provenance, representation, compatibility and placement. | V0 has no new trainable weights and does not depend on learned-head infrastructure. |
| D — Dynamic Composition | Future exact backbone + probe/head composition identity, compatibility, rollback and invalidation. | No arbitrary Python classifier, `.pt` sidecar or model-name hook. |
| N — Native Cognitive State | Participates only when a future readout consumes R/E under an exact StateProfile. | V0–V3 do not depend on Program N/B1; Decision Readout is not E, State Update or semantic memory. |

Ordinary model/runtime capability truth must eventually advertise a non-stateful
readout without requiring StateProfile. A future R/E-aware StateProfile may
describe readout mode, required state/profile generation, read site,
probe/head identity, candidate bounds, dtype/layout, batching and resource
envelope; no public StateProfile field or ABI is frozen here. A read-only future
invocation may preserve `E_after == E_before`.

Minimum Sufficient Cognition remains YAI policy: YAI chooses deterministic
logic, Decision Readout, generative reasoning or human/review escalation. YVEX
may expose exact compatibility, score/calibration semantics, candidate bounds,
batching and measured latency, memory, GPU time, compute or energy. It does not
choose the semantic mode. The future evaluation objective is
**time-to-qualified-solution**, comparing ordinary generative control with
adaptive cognition while holding machine, base model, task and tools constant
where possible. Report wall time, generative calls/tokens, prefill work,
readout/tool calls, redundant work, peak memory, GPU time, energy where
available and independently verified correctness; this doctrine earns no
current performance claim.

The external pressure is a real bounded YAI semantic Decision Plane and typed
candidate frontier, but YAI's `CognitiveDecisionRequest v1`,
`CognitiveDecisionDistribution v1` and `CognitiveDecisionFrontier v1` remain
YAI-owned application/domain contracts. YVEX now has one internal computational
Decision Readout producer, but YAI has not selected it as a consumer; a shared
public producer/consumer ABI therefore remains premature. I07 remains
UNSELECTED. Only an actually selected cross-project consumer would trigger
normal BOUNDARY/Interlock evaluation.


## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
