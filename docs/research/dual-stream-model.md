<!-- docs:metadata
title: Slow-Update Dual-Stream Model
id: yvex.research.dual-stream-model
document: research
status: research
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Slow-Update Dual-Stream Model

**N.B1 selects a research target, not a deployed cognitive-state capability.**

[Up](README.md)

> **TARGET / RESEARCH.** This page owns questions and selected target doctrine.
> [Status](../project-control/STATUS.md) owns demonstrated capability;
> [Tasks](../project-control/TASKS.md) owns authorized delivery.

## Question and qualification

N.B1 selects a research target, not a deployed cognitive-state capability.

Neutral-path surgery must preserve the base computation before learned state is evaluated. Persistent-prefix controls must be measured separately from useful model-native state.


## N.B1 Slow-Update Dual-Stream

**Official primary Program N research target: N.B1 — Slow-Update Dual-Stream.**
Program N remains OPEN. B1 is full dual-stream from the beginning: `R` is the
primary residual/activation/current-computation stream; `E` is the persistent
Experiential Computational State stream. Slow Update describes E's update
clock, not ordinary persistent KV.

```text
R_(l+1) = F_l(R_l, E_g)
E_(g+1) = G_g(E_g, R_source)

l = primary layer/computation index
g = experiential update generation inside the current execution

R_l ∈ dtype_R [B, T, d_R]
E_g ∈ dtype_E [B_E, M, d_E]
```

M is independent of current prompt length T; d_E and dtype_E may differ from
d_R and dtype_R. E need not be text, token-aligned or carry ordinary token
positions. Its precision/layout, update frequency and lifetime may differ from
R and from local sequence state. These dimensions are not public ABI.

F/G remain architecture-neutral: future qualified realizations may use
cross-state attention, latent interaction, gated low-rank projection, recurrent
or SSM transitions, associative memory, sparse routing, fast-weight/neural
memory or another qualified architecture. The first B1 model selects latent
slots, cross-state read, a learned primary gate and slow bidirectional update.
It does not freeze the generic Program N equations.


## B1-v0 reference StateProfile and structural budget

**REFERENCE DESIGN — NOT CURRENT IMPLEMENTATION — NOT MEASURED PERFORMANCE —
NOT FROZEN ABI — NOT FINAL MODEL ARCHITECTURE.** These numbers define the first
falsifiable B1 design, not universal Program N fields or performance estimates.

| Reference profile fact | B1-v0 choice |
| --- | --- |
| First trained research vertical | Current admitted Qwen text target, with its untouched backbone retained separately. |
| Primary stack | 64 layers: 48 recurrent/gated-delta mixers and 16 full-attention layers; d_R = 5120. |
| Experiential slots / width | M = 64; d_E = 1024. |
| Cross-state interaction width | d_X = 512. |
| Canonical E dtype | BF16. |
| State Read sites | One per primary text layer: 64 sites. |
| Slow State Update sites | Eight boundaries over 64 layers, approximately one per eight layers. |
| Input / state-producing execution | State Read plus configured slow intra-model update. |
| Ordinary autoregressive decode | State Read enabled against a stable bound experiential generation/snapshot; persistent State Update disabled by default. |
| Cross-request publication | Final candidate → prepare → transactional commit only. |

For one batch/state instance, E shape `[64, 1024]` contains 65,536 elements:
`64 × 1024 × 2 = 131,072` BF16 bytes = **128 KiB**. Holding E_committed,
E_working and E_candidate simultaneously is approximately **384 KiB** of
canonical E tensor storage, excluding metadata and derived materializations.
This is structural arithmetic, not observed allocation or an allocation policy.

| Large projection at each reference State Read site | Shape | Parameters |
| --- | --- | ---: |
| W_Q_RE | [5120, 512] | 2,621,440 |
| W_K_E | [1024, 512] | 524,288 |
| W_V_E | [1024, 512] | 524,288 |
| W_O_ER | [512, 5120] | 2,621,440 |

| Reference projection budget | Derived total |
| --- | ---: |
| State Read core per site | 6,291,456 parameters |
| 64 State Read sites | 256 large tensors; 402,653,184 parameters |
| Eight symmetric State Update sites, four large projections each | 32 large tensors; 50,331,648 parameters |
| Combined projection core | 288 large tensors; 452,984,832 parameters |
| BF16 core projection storage | 905,969,664 decimal bytes; 864 MiB binary |
| Complete augmentation planning range | Approximately 455M–500M new parameters until the exact module graph freezes |

The symmetric update estimate uses E→interaction query `[1024,512]`,
primary→interaction key/value `[5120,512]` each and interaction→E output
`[512,1024]`. Totals exclude gates, state normalization, learned slot
identities/embeddings, initializer/State Encoder and small control parameters.
The complete planning range is a low-single-digit-percent augmentation of the
Qwen backbone, not a second full backbone or a measured implementation claim.

StateProfile is the broader model-specific doctrine: representation class,
geometry/dtype, initializer/State Encoder capability, read/update sites,
interaction width/head geometry, projection/gate structure, normalization,
update clock, state-producing modes, decode-write posture, commit boundary,
derived materializations, residency constraints, checkpoint compatibility,
Reconcile capability and exact augmentation/model compatibility. B1-v0 chooses
one profile; it does not prescribe a universal record containing every choice.


## Reference State Read and neutral-path surgery

At each reference Qwen primary layer:

```text
H_l = Norm_R(R_l)
A_l = PrimaryMixer_l(H_l, ordinary sequence state)

Q_R = Project_Q_RE(H_l)
K_E = Project_K_E(E_g)
V_E = Project_V_E(E_g)
B_l = CrossStateRead(Q_R, K_E, V_E)

g_R = learned primary gate
R_intermediate = R_l + A_l + g_R ⊙ Project_R(B_l)
R_(l+1) = R_intermediate + FFN_or_MoE(Norm(R_intermediate))
```

Exact normalization/residual placement is model/composition-specific; this is
the first B1 reference architecture, not a universal Transformer law.
For surgery parity before training, initialize the primary contribution with
`g_R ≈ 0`, experiential `write ≈ 0` and `retention ≈ 1`. A disabled/neutral
path must support exact or explicitly qualified near-exact base preservation;
near-zero gates alone are not evidence of parity. Gates remain lightweight.


## Reference Slow State Update and execution clock

At a declared update boundary:

```text
H_E = Norm_E(E_g)
H_R = selected primary update source
Q_E = Project_Q_ER(H_E)
K_R = Project_K_R(H_R)
V_R = Project_V_R(H_R)
C_g = CrossStateUpdate(Q_E, K_R, V_R)

retain_g = learned retain gate
write_g = learned write gate
proposal_g = Project_E(C_g)
E_(g+1) = StateNorm(retain_g ⊙ E_g + write_g ⊙ proposal_g)
```

This is a first research equation, not the universal update law. The parity
configuration must also preserve retention through StateNorm or bypass the
write path; a normalization that changes E cannot silently count as no update.
R advances at ordinary model frequency, E at the slower model-defined clock:

```text
layers  0–7  read E_0 → slow update E_0 → E_1
layers  8–15 read E_1 → slow update E_1 → E_2
...
layers 56–63 read E_7 → final update E_7 → E_candidate
```

This trajectory belongs to configured input/state-producing execution.
Ordinary token decode initially reads a stable snapshot; persistent E writes
on every generated token are not the default. At the end of a qualified
state-producing invocation/run, a final candidate may be offered for
transactional publication. Decode-time E mutation requires separate future
research and evidence. This execution-mode distinction is architectural.

Latent slots are computational positions, never YAI fact, history, policy,
user or long-term-memory slots. Learned slot identity, embeddings,
specialization, competition and routing may emerge; YAI retains semantic
categories and admission.


## Next reads

[System architecture](../architecture/README.md) · [Evidence](../evaluation/README.md) · [Long horizons](../../ROADMAP.md)
