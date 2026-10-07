<!-- docs:metadata
title: Native Computational State
id: yvex.research.native-computational-state
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Native Computational State

**Can model-native continuity survive context replacement without taking semantic authority?**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

Can model-native continuity survive context replacement without taking semantic authority?

State usefulness, retention, interference and compatibility need independent controls. Current recurrent/KV/prefix machinery is a foundation, not an implementation of Program N.


## Semantic authority and computational realization

The adopted YAI-side baseline is **D/H/S Recall v2 implemented and qualified
at its published bounded scope**. This is the external architectural premise,
not a YVEX qualification result. The next YAI boundary,
`RECALL-AWARE.WORKING.STATE.0`, remains next/unselected: there is no public
Semantic Working State `W` producer contract and no public YAI↔YVEX W→E wire
format. Recall `R_t^q` below is semantic recall, distinct from model stream `R_l`.

```text
                           YAI
sources ──► D_t ┐
history ──► H_t ├──► qualified Recall v2 ──► R_t^q   IMPLEMENTED, bounded
current ──► S_t ┘                              │
                                              ▼
S + R + task + authority + Case constraints ──► W    NEXT YAI TARGET
                                              │
                         future public semantic/computational boundary
                                              │
                                              ▼
                           YVEX PROGRAM N
                         exact model + StateProfile
                                              │
                                     Lower / Reconcile
                                              │
                                              ▼
                                         E_committed
                                              │ begin
                      PRIMARY STREAM          EXPERIENCE STREAM
                           R_l                      E_working_g
                            ├──── State Read ◄─────────┤
                            ▼                          │
                          R_l+1                        │
                            └── selected slow update ─► E_working_g+1
                                                       │
                                                       ▼
                                                  E_candidate
                                                       │ prepare
                                                 commit / abort
                                                       │ commit only
                                                       ▼
                                              next E_committed

separate deliberation continuity: L_t,k = unfinished computation
W ingress, Lower/Reconcile and the R/E execution below W remain FUTURE.
```

| Owner | Authority |
| --- | --- |
| YAI | Source-grounded domain knowledge D, history H, current semantic state S, Recall, working-state selection, semantic meaning, relevance, source truth, authority, disclosure, Case continuity, assignment semantics and semantic admission. |
| YVEX | Exact model-state capability, StateProfile, computational realization and model-specific identity, compatibility, physical execution, residency, lifecycle, checkpoint and computational evidence. |
| Model | Learned interpretation, learned R↔E interaction, learned State Read and learned State Update. |

**Semantic memory != computational memory; semantic truth != computational
state; model update != semantic admission.** YVEX may return computational
consequences, state transitions and evidence; it never becomes semantic authority.
YAI does not own tensors, CUDA, residency, layers or model update equations.
E does not become YAI memory, and model-produced latent changes do not become
canonical Case facts.

[Native adaptation/post-training](model-adaptation.md#native-adaptation-and-post-training-horizon)
is a separate future YVEX computational target. Updating learned parameters is
not E continuity, Recall, semantic admission or the default solution to a long
Case. YAI must explicitly select and authorize any Case-derived training material;
no automatic Case-to-weights policy or YAI-specific trainer is adopted.

Future W ingress is **mechanical validation, not semantic adjudication**.
YVEX may validate representation integrity, schema/version compatibility,
identity/digest binding, declared provenance binding, exact model compatibility,
StateProfile and state-generation compatibility, realization capability and
physical feasibility. It must not re-evaluate W's semantic validity, Participant
authority, source visibility, disclosure permission, recalled relevance, policy
currency, documentary truth or semantic admission. This creates no second
reference monitor in YVEX.

No `StateFrame` ABI, `StateDelta` ABI, W serialization, YAI endpoint, YAI-specific
runtime adapter or fixed cross-repository schema is adopted here. A real future
producer/consumer contract must use the permanent BOUNDARY workflow.
This target-doctrine refinement is not a BOUNDARY event.


## E, L and three continuity classes

`E_t` is Experiential Computational State: reusable model-native computational
experience, potentially cross-request and longer-lived than a session, with
independent identity/generation, StateProfile binding, model-specific
representation, rebuildability, residency and checkpointing.

`L_t,k` is Latent Deliberation State: unfinished computation for an authorized
execution at internal step k. A suspended execution may require the bound E
generation, current E_working if applicable, primary/intermediate execution
state, program/iteration position, RNG state, runner state, execution
configuration and checkpoint metadata. This collection preserves deliberation,
not experiential memory. Future checkpoint/resume/invalidate/discard of L is
independent from committed E. Loss of L loses unfinished work; it must not lose
YAI semantic memory and need not lose E. Common paging/checkpoint mechanisms
do not merge L into E or introduce a second thinking runtime.

| Continuity class | Owner / survival contract |
| --- | --- |
| Semantic continuity | YAI; authoritative semantic state survives model/runtime loss. |
| Experiential computational continuity | E; reuse requires exact model/program/profile/state compatibility. |
| Deliberation continuity | L; unfinished work resumes only under an execution compatibility contract. |

Model replacement may invalidate E and L; it must not invalidate YAI semantic
continuity. No generic cross-model latent portability is claimed.

Local/sequence state comprises attention KV, gated/recurrent and convolution
state, position, RNG, decoder state, speculative candidates and ordinary
prompt/session continuation. E is separately identified, versioned and
invalidated, StateProfile-bound and potentially derived from future external W
realization. A prompt/session reset need not invalidate E. A W, StateProfile,
executable-composition or model change may invalidate E without changing YAI
semantic state. An ordinary engine/session checkpoint is not qualified L
continuity.


## Lower, Reconcile and model State Update

| Transformation | Target meaning | Refusal / authority boundary |
| --- | --- | --- |
| `E = Lower(W, exact model/composition, StateProfile)` | Full realization of future externally selected W. | Requires an actual qualified ingress/producer contract. |
| `E' = Reconcile(E, qualified external ΔW, exact model/composition, StateProfile)` | External W change drives computational refresh. | Optional per profile; may refuse and require full rebuild. |
| `(R, E) → model computation → candidate E'` | Learned State Update caused by model computation. | Produces a computational candidate, never semantic admission. |

External Reconcile and model State Update must not collapse into one generic
update. An exact realization binds external-source provenance/digest, exact
executable composition/artifact/binding, StateProfile, compiler/lowering,
precision/layout, generation, parent/update/checkpoint lineage and compatibility
posture sufficiently to decide reuse, invalidation or rebuild. These are
lifetime/identity requirements, not a wire schema or a mandatory giant hash.


## Generic provider and program ownership

```text
ordinary mode:
  input → ordinary admitted model execution → output

future state-capable mode:
  input + optional compatible E binding + exact StateProfile
    → state-aware execution → output + optional E_candidate
```

Ordinary mode requires neither E nor YAI. Numerical execution must not require
Case, Recall, Participant, YAI identity or semantic-authority objects and must
never branch on `if caller == YAI`. YAI is a future rich consumer of a generic
capability. No QwenMemoryRuntime, DeepSeekMemoryRuntime or YAIStateRuntime.

| Program | Future responsibility | Explicit non-ownership |
| --- | --- | --- |
| C — Model Language & Compiler | R/E semantics, state inputs/outputs, Read/Update blocks, versions, root effects, DAG/barriers, state-aware entrypoints and trainable augmentation meaning. | Semantic authority, residency and optimizer policy. |
| N — Native Cognitive State | StateProfile, E realization identity, qualified future ingress, Lower/Reconcile, compatibility, invalidation/rebuild, Read/Update realization, experiential provenance and computational evaluation contract. | Case meaning, semantic admission and physical allocation/scheduling. |
| S — Sequence Runtime | E committed/working/candidate lifetime, bindings/leases, transactions, paging/COW/sharing, residency/movement, checkpoints/rollback, resource accounting, safe schedule and future L checkpoint/resume. | Learned equations or semantic interpretation of banks. |
| D — Dynamic Composition | Augmentation/module source, exact base compatibility, trained augmentation identity, State Encoder/Adapter and LoRA/DoRA/selective-adaptation composition, attach/import/deployment identity. | Invented compatibility or a mandatory optimizer. |
| R / Source owners | Exact base source, initial augmentation weights, dataset and recipe provenance, checkpoint lineage and final trained source identity. | Training objectives as inference authority or copied source ownership. |
| P — Physical Model Compiler | B1 weights and E precision/layout, derived representations/materializations, backend compatibility and target-machine-aware admitted choices. | Semantic meaning of W/E or training policy. |
| Q — Qualification | Surgery parity, ordinary preservation, held-out post-training evaluation, causal state usefulness, interference/retention/update behavior, checkpoint, reference/CUDA equivalence, hybrid integration and cross-model controls. | Treating training/selection evidence as held-out or release proof. |

These extend existing owners, not a new strategic program or directory layering.
Model mathematics remains model-owned; N's realization contract, S's physical
lifetime and P's representation choices remain distinct.



## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
