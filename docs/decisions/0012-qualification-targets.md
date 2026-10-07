<!-- docs:metadata
title: 0012 — Identity-bound qualification and product evidence
id: yvex.decisions.0012-qualification-targets
document: reference
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0012 — Identity-bound qualification and product evidence

**Accepted structural selection; the first model instantiation is still incomplete.**

## Context

An isolated target-only synthetic run and an operator's speculative multi-turn
run measure different products. Encoding parity does not establish full-model
reference agreement. A family name or a single throughput value cannot carry
the provenance needed for a model, physical variant or deployment claim.

## Decision

Qualification targets reference canonical source, checkpoint, Transformation IR,
physical policy, artifact, binding, specialization, build, backend and topology
identities. They also bind configuration, workload and product path. Missing
identities stay explicit; ephemeral process/path/time facts are provenance only.

The qualification envelope does not replace runtime benchmark v5, numerical
oracles, QA receipts or artifact admission. It links their exact evidence into
independent family, checkpoint, representation, backend execution, deployment
performance and product planes. Backend execution has its own claim state;
a completed inference measurement does not automatically qualify its numerical
or lifecycle gates. Publication origin and evidence state
are orthogonal: a published characterization is not QUALIFIED, and a local
receipt is not a maintainer guarantee.

Manifests own workload cases and admissible reasoning/strategy axes. Comparison
requires matching metric-specific keys; an explicit experiment declares its
changed axes. Quality regression cannot cross checkpoint/reference corpus.
Reference distributions are not inferred from top-k or generated prose.

`tools/qualification.py` owns rules and their generated machine projection.
The existing benchmark publication owner generates public matrix/detail views.
Rust owns operator parsing/presentation and consumes these same projections;
native measurement uses the existing typed client, not a second wire protocol.
The operator registry remains the only command grammar authority. Reports follow
the permanent [agent protocol](../../AGENTS.md#model-representation-and-performance-qualification-reports).

## Consequences

Targets form a sparse matrix, not a Cartesian promise. DeepSeek is the first
instantiation; other families add packs, not new reporting architectures. Every
published quantization claim is separately addressable. None/high/maximum and
target-only/DSpark are separate cells. Native product and HTTP measurements stay
distinct from controlled-engine experiments. Generated documentation is part of
qualification, not an optional prose summary.

Independent-producer capture is a generic adapter over family-prepared token
inputs. The family owner authenticates encoding and interprets output grammar;
the adapter authenticates the executable, libraries and actual request/response.
Reference-only container projections retain immutable recipes and readback
evidence without becoming admitted runtime representations. Multi-turn inputs
bind actual independent assistant history and refuse malformed or incomplete
source grammar rather than inserting delimiters to complete a fixture.

Independent full-model references, representative product measurements and
throughput gates remain required by the active Task. This decision does not
earn them, change numerical classes, add remote mutation or introduce signing,
certification infrastructure or a quality leaderboard.

## Alternatives rejected

- One aggregate PASS: hides missing independent evidence.
- Model-name-only benchmark records: allow claims to escape their configuration.
- Hand-maintained public numbers: drift from machine evidence.
- Separate family-specific benchmark databases: repeat comparison and lineage bugs.
- Treating a locally produced artifact as officially qualified: confuses support,
  execution and publication authority.
