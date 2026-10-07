<!-- docs:metadata
title: 0013 — Native model adaptation ownership horizon
id: yvex.decisions.0013-native-model-adaptation-horizon
document: reference
status: current
owner: research
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0013 — Native model adaptation ownership horizon

**Accepted future ownership selection; no runtime implementation or delivery selected.**

## Context

The existing adaptation doctrine preserves immutable bases, trainable manifests,
dataset/recipe provenance, checkpoint lineage and independent qualification.
Its external-trainer bootstrap must not be read as excluding native training
from YVEX or assigning computational post-training to YAI. Low-memory adapter
streaming is not evidence for full-parameter training in the same envelope.

## Decision

YVEX owns future generic computational adaptation/post-training. YAI owns
semantic material selection and authority, privacy, visibility and policy;
Studio presents/orchestrates admitted contracts. External differentiable
trainers remain valid bootstrap/reference producers. No YAI-specific trainer
or cross-repository wire schema is selected.

Future training extends existing source, compiler, transformation/physical,
composition, runtime/resource, backend and evaluation ownership. Distinct
training classes retain exact trainable scope and candidate/checkpoint lineage.
Layer streaming is one possible realization under the existing OPEN phase-aware
resource-lifetime target, not another architecture or a mandatory strategy.
Backward IR, optimizer ABI and implementation schedule remain undecided.

The [research owner](../research/model-adaptation.md#native-adaptation-and-post-training-horizon)
owns the detailed doctrine and pinned Soup reference. The existing qualification
system binds trained candidates and their independently earned evidence; loss,
saved adapters or successful load do not constitute qualification.

## Consequences

ROADMAP gains a future product horizon; Status remains OPEN. Selected Tasks and
A03 READY are unchanged. There is no trainer code, kernel, CLI, public ABI,
protocol or production-membership change. Case continuity/Recall remains
source-addressable semantic state, not silent learning into weights.

## Alternatives rejected

- Assigning training execution to YAI: mixes semantic authority with computation.
- Mandating a separate trainer/runtime: bypasses existing identity/lifetime owners.
- Equating frozen-base streaming with full training: conceals gradient/optimizer costs.
- Treating training completion as qualification: omits held-out and deployment evidence.
