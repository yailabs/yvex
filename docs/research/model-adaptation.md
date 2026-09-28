<!-- docs:metadata
title: Model Adaptation and Qualification
id: yvex.research.model-adaptation
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Model Adaptation and Qualification

**What evidence would justify training and admitting a state-aware model composition?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

What evidence would justify training and admitting a state-aware model composition?

Keep data, recipe, base/checkpoint and augmentation provenance. Separate surgery parity, fitting, held-out usefulness and negative ablations; training data cannot qualify itself.


## Qwen B1 model adaptation and post-training target

Qwen is the first **trained B1 research vertical**, not a newly selected wave.
The [current family record][families] identifies `Qwen/Qwen3.8-27B`, interpreted
by the existing `qwen3_5` owners: 64 text layers, 48 recurrent/gated-delta mixers
with convolution state and 16 full-attention layers, primary width 5120.
B1 attaches through a generic primary-layer interaction boundary:

```text
attention layer:   R → original full attention / local sequence state
                  R → State Read(E)

gated-delta layer: R → original recurrent / convolution computation
                  R → State Read(E)
```

Both mixer classes use the same B1 abstraction and physical lifecycle. This
hybrid reference would test directly that E is not another KV cache; no
separate experiential runtime is permitted for either mixer class.

The untouched Qwen checkpoint remains an immutable, separately executable
reference. B1 is a distinct exact composition: **immutable backbone + B1
architecture/composition + B1 trained weights + StateProfile**, with its own
source/composition identity, training provenance, artifact, deployment,
execution identity and qualification. The original model is not retroactively
renamed or converted into B1.


[families]: ../model-families/integration.md

## Mandatory model controls and adaptation ladder

These are future research controls, not current execution capabilities,
scheduled waves or expected outcomes. Q4 is conditional, not automatic.

| Control | Architecture / weights / state | Question isolated |
| --- | --- | --- |
| Q0 — Original Qwen | Original architecture and weights, ordinary execution, no E. | Base reference. |
| Q1 — Standard-memory Qwen | Original architecture/weights; equivalent relevant information in ordinary context and/or qualified retained prefix/KV. No learned B1 path. | Same-backbone standard-memory baseline. |
| Q2 — B1 Surgery Parity | Full B1 graph and E present; new weights initialized, primary read gate neutral/near-zero and update write neutral/near-zero; no post-training. | Q0 vs Q2: does surgery preserve the original path? |
| Q3 — B1 Post-Trained / Frozen Backbone | Backbone frozen; train State Encoder/Initializer, State Read, State Update, gates, experiential normalization and B1-specific parameters. | Q1 vs Q3: does learned E improve over standard memory on the same backbone? First actual trained B1 model. |
| Q4 — B1 + Selective Backbone Adaptation | Only if Q3 warrants bounded LoRA, DoRA or selective unfreezing alongside B1. | Q3 vs Q4: does backbone co-adaptation materially help? |
| D0 — DeepSeek Standard Control | Original DeepSeek architecture/weights, ordinary YVEX execution, no B1; equivalent information through ordinary rendered context and/or qualified retained prefix/context. | Q1 vs D0: what does scale buy under the standard paradigm? Q3/Q4 vs D0: can persistent computational state recover capability otherwise requiring more scale/context? |

Potential Q4 sites include selected normalizations, mixer output projections,
attention projections, FFN/down/output interaction points and later block
groups. Selection remains evidence-driven; the roadmap does not freeze sites.

| Stage | Trainable scope | Required research question / boundary |
| --- | --- | --- |
| PT0 — Surgery parity | No learning required. | Disabled/neutral B1 contribution preserves the backbone path, with exact or declared near-exact numerical criteria. |
| PT1 — B1-only post-training | Frozen backbone; B1 augmentation trainable. | Demonstrate causal use of E. |
| PT2 — Selective backbone adaptation | B1 plus limited backbone plasticity. | Test whether pretrained representations benefit from co-adaptation to R↔E. |
| PT3 — Broader architecture-aware post-training | Future broader adaptation only if earlier stages show a clear ceiling. | Not scheduled merely because listed. |

Native training around R/E from the beginning is a deeper research horizon,
not a B1 prerequisite.

The [live DeepSeek/DSpark record][deepseek] owns D0 structural truth: 43-layer
SWA/CSA/HCA hybrid target, width 4096, mHC with four 4096-wide streams and a
16,384-wide native residual boundary, 256 routed experts with top-6 selection,
one shared expert per target layer and the admitted DSpark target/draft
composition. D0 asks how much a substantially larger unmodified model can do
with equivalent information delivered conventionally. It does not test whether
DeepSeek could benefit from B1 training.

**Do not insert random untrained B1 weights into DeepSeek and call it D0.**
That confounds surgery damage with lack of training. DeepSeek is not the first
post-trained B1 target. A future DeepSeek-B1 variant is unscheduled and may be
considered only after causal Qwen evidence warrants extension; it would need
its own neutral-path surgery parity before training. No DeepSeek B1 tensor
budget or training campaign is adopted.


[deepseek]: ../model-families/deepseek-v4-flash.md

## Provenance and inference composition pipeline

```text
immutable base model source
  + architecture augmentation definition
  + StateProfile
  + trainable-parameter manifest
  + training dataset identity/provenance
  + training recipe identity
  + optimizer/training configuration identity where required
  + checkpoint lineage
    → trained augmentation / adapted weights
    → exact base compatibility
    → exact executable composition identity
    → YVEX import / compilation / physical representation
    → runtime binding
    → independent qualification
```

This strengthens the existing D/C/R/P/Q lane without promoting generic adapter
or external post-training integration to implemented. Training provenance
authenticates where weights came from; it does not replace inference admission
or independent qualification.

An initial external differentiable research trainer may own autograd,
backpropagation, optimizer, gradient accumulation, schedule and training
checkpoint production with exact input/output provenance. YVEX imports the
trained composition as immutable source truth. A YVEX-native trainer is neither
implemented nor required: backward IR, optimizer execution, training scheduling,
gradient state and distributed training remain possible future research.
C may declare trainable roles and computational meaning without owning optimizer
policy; D composes compatible modules, Source retains provenance, P realizes
weights and Q independently qualifies the result.


## Cross-execution episodes and objective families

B1 training/evaluation must exercise genuinely cross-execution state, for example:

```text
run 1: information introduced
run 2: additional state
run 3: contradiction / supersession
run 4: delayed dependency
run 5: irrelevant distractor
run 6: changed condition
final task: selective retained experience required
```

Candidate task classes include cross-request entity binding, supersession,
long-gap dependency, technical project continuity, contradiction tracking,
persistent preferences with distractors, procedural progress and delayed
information composition. These are research episodes, not YVEX product or
slot semantics. Equivalent-information context/prefix controls must remain
visible, alongside held-out evaluation distinct from training and selection.

Possible objective families combine ordinary language/task learning,
teacher-logit or hidden-state distillation, state usefulness, wrong-state
contrast, retention/stability regularization, write-magnitude regularization
and base-model behavior preservation. No coefficients or final recipe are
frozen. Success is not maximum write magnitude/frequency: useful E may retain
most of its state and change only a small fraction.



## B1 qualification controls

Mechanism existence is not B1 success. The [Q0/Q1/Q2/Q3/Q4/D0 matrix](#mandatory-model-controls-and-adaptation-ladder)
must remain individually reported; Q4 is conditional on earlier evidence.
Training metrics, held-out behavior, numerical conformance, runtime lifecycle,
performance and release evidence remain separate. No outcome is predicted.

| Future qualification boundary | Mandatory controls / observations |
| --- | --- |
| Ordinary-mode and surgery preservation | Ordinary execution without E; Q0/Q2 exact or declared near-exact parity; state-disabled behavior and base-model regression. |
| Causal State Read usefulness | Correct E vs zero E, wrong E and swapped E; State Read disabled. Show whether E carries useful information and whether the model actually depends on it. |
| Freshness, retention and update | Fresh vs stale E; cross-request retention; State Update disabled; write-frequency ablation; stability, intended forgetting and measured update behavior. |
| Transaction and deliberation boundaries | Candidate/prepare/commit/abort transitions, cancellation, unchanged committed generation on failure; checkpoint/recovery, with future L continuity qualified independently. |
| Compatibility and materialization | StateProfile mismatch refusal, model/composition replacement, E invalidation/rebuild and derived-cache staleness/rematerialization. |
| Numerical and hybrid integration | Independent reference vs CUDA operators and composed execution; Qwen's recurrent/convolution and full-attention layer paths share the B1 boundary. |
| Comparative behavior | Q1/Q3 same-backbone standard-memory comparison, conditional Q3/Q4 co-adaptation, Q1/D0 scale control and Q3/Q4 vs untouched D0 with equivalent relevant information. |
| Performance and resources | Once implemented: repeated throughput/latency distributions, memory/residency, preparation/rematerialization costs, update frequency and numerical effect on exact source/composition/profile/device/workload identities. |

Correct, zero, wrong, swapped and stale E, State Read disabled and State Update
disabled are mandatory explicit state-level ablations, never hidden in an
aggregate score. Report expected/observed tokens, logits/state values or error
metrics where applicable, reference identity, tolerances, worst cases and
dispersion. Internal agreement does not establish upstream conformance;
training-set improvement does not establish held-out state usefulness.



## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
