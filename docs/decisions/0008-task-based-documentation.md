<!-- docs:metadata
title: 0008 — Task-based documentation architecture
id: yvex.decisions.0008-task-based-documentation
document: reference
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0008 — Task-based documentation architecture

**Separate current state, selected delivery and long-horizon direction.**

[Up](README.md)

Date: 2026-09-28
Status: accepted
Supersedes: [0001 — Public project control](0001-public-project-control.md).

## Context

ROADMAP accumulated current capability, architecture, research, delivery history
and release detail. The temporary research bootstrap also repeated old state.
One control surface could no longer represent those distinct authorities clearly.

## Decision

Status owns bounded current maturity. Tasks owns significant delivery progression,
selected work and earned exits. Root ROADMAP survives only as macro horizons and
convergence gates. ADRs remain the structural decision owner; no parallel
DECISIONS file is created. Memory is derived re-entry context.

Canonical Markdown, hidden metadata and README landings form one documentation
system. HTML/PDF and benchmark figures project their sources. Contracts, Model
Families and Releases remain first-class YVEX profile areas.

## Consequences

Substantial engineering starts from selected Tasks and reconciles architecture,
contracts, evidence and control before completion. Issues and PRs organize work
and review but cannot independently change Task or capability truth. Historical
IDs survive without retaining every chronological planning artifact.

## Alternatives

Keeping ROADMAP as a mixed encyclopedia preserves duplicate update paths.
Copying YAI's folder ontology hides YVEX's exact contracts, families and releases.
Both alternatives are rejected; the common grammar is adopted with local owners.
