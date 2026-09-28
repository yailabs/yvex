<!-- docs:metadata
title: Documentation Protocol — YVEX Profile
id: yvex.reference.documentation-protocol
document: reference
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Documentation Protocol — YVEX Profile

**Shared epistemic grammar; compiler/runtime repository ontology.**

[Up](README.md)

This profile adopts the current [YAI Documentation Protocol](https://github.com/yailabs/yai/blob/master/docs/reference/DOCUMENTATION-PROTOCOL.md)
and specializes it for YVEX. The migration inspected YAI master at
`702375e569d1befb74bcf9bc9acbf73b9b117589`; the living common owner remains YAI.
This page owns local specialization, not a frozen competing common protocol.

## Authority and roles

**One fact → one canonical owner → many references/projections.**

| Question | YVEX owner |
| --- | --- |
| Why / what are we building? | Product Vision / PRD |
| How does the system work? | Architecture landing and plane dossiers |
| What must remain true? | Architecture Invariants |
| What exact producer/consumer behavior is required? | Contracts |
| What is family-specific? | Model Families |
| Where are we / what is selected? | Status / Tasks |
| Which long horizons converge? | Root ROADMAP, read for planning only |
| Why was a structural choice selected? | Numbered ADRs; no parallel DECISIONS file |
| What is expensive to rediscover? | Derived Memory |
| How do I operate it? | Guides |
| What did we demonstrate or measure? | Evaluation / benchmarks / release evidence |
| What are we investigating? | Research |
| Where is exact lookup? | Reference |

Git retains forensic chronology. Tasks retain enough significant completed
delivery to explain the present. Generated HTML/PDF and navigation are views;
structured observations are evidence, never a capability database.

## Hidden document metadata

Human-facing canonical Markdown begins with `<!-- docs:metadata` and closes the
YAML metadata comment with `-->`. Visible YAML front matter is forbidden.
The visible body begins with its Markdown title, thesis and useful content.
The root product README may use its accessible branded logo as page identity;
this exception does not remove headings from technical documents.
No inline CSS, script or raw page header is required to read a fact.

The controlled schema is [document.schema.json](document.schema.json).
Required fields: `title`, `id`, `document`, `status`, `owner`, `audience`,
`publication`. Optional fields: `plane`, `related`, `generated`, `source`.
Each field has a consumer: discovery, validation, related routing, publication,
plane navigation or projection provenance. No free-form tags or capability assertions.

Document roles use the common product/architecture/architecture-plane/project-control/
guide/evaluation/research/reference vocabulary. Contracts, families and ADRs
retain first-class folders while using lookup/reference presentation metadata.
ADR adoption state remains in its decision body, distinct from document posture.

## Three reading experiences

- **A — Product / Control:** thesis, bounded state or delivery progression first.
- **B — Architecture / Research:** system position, ownership and flow before exact mechanisms.
- **C — Reference / Evidence:** scope, lookup paths, stable headings and exact detail.

Use prose for reasoning, diagrams for relationships, tables for comparison,
flows for sequence and charts for measurements. Major architecture pages expose
current behavior before labeled open/target frontiers. Review the middle and
end as well as the first viewport. Do not shorten away technical qualifiers.

Full capability matrices remain visible below a compact overview. Optional
completed Task exits may use `details`; current selection, blockers and authority
boundaries may not depend on opening a disclosure panel.

## Task and promotion grammar

Program → Phase → Task → implementation checklist. A Task Pack temporarily
groups selected Tasks; it does not add another persistent level. Historical
Wave IDs keep their identity. Independent discovered outcomes become Tasks;
steps required by the same outcome remain implementation details.

Task states: ✅ COMPLETE, 🔵 IN PROGRESS, ⬜ READY, ⛔ BLOCKED.
Capability states: 🟢 ESTABLISHED, 🟡 PARTIAL, 🔴 OPEN, ⚪ LATER.
Task counts derive from canonical Task rows. Maturity counts derive from the
existing canonical capability rows; their denominator is not product completion.
The matrix may evolve only through an evidenced capability change.

Implement → test → observe → retain evidence → consider promotion.
Software tests, numerical conformance, artifact/runtime/device evidence,
component benchmarks, model behavior and releases remain distinct. Independent
upstream evidence is different from internal agreement. Missing mandatory
evidence is BLOCKED/SKIP, never PASS. A complete Task does not select its successor.

## Diagrams and publication

YVEX retains one editable JSON source for each system diagram. Typed nodes,
explicit relationship endpoints and ownership groups generate native Mermaid
blocks in canonical Markdown and deterministic SVG for offline HTML/PDF.
The Markdown blocks are checked projections, not independent diagrams to edit.
GitHub renders Mermaid; static links remain available for terminal viewers.

The same graph preserves source, logical, physical, runtime, state, device and
external boundaries across layouts. Text labels carry semantic roles; violet
is an accent, not a capability signal. HTML may add focus/zoom. No CDN or
JavaScript is necessary to read an offline or PDF figure.

The reader derives its navigation from the repository tree, README links and
document metadata. It may project badges, search, filters and themes, but it
cannot become a second content owner. Reduced motion and keyboard access apply
to enhancements. Generated files belong in ignored build output except checked
static figures/tables whose sources and drift checks are explicit.

## Evidence data is different

Benchmark JSON contains observed experimental values and exact run context.
Document metadata contains publishing/routing facts. Neither can automatically
promote Status. The [benchmark methodology](../evaluation/benchmarks/methodology.md)
defines comparable observations and the relationship to existing runtime records.

## Links, discovery and validation

Use relative links inside the repository; use living canonical links for current
external contracts and commit-pinned links for immutable evidence. Do not publish
private addresses, SSH aliases or incidental machine paths as architecture identity.
Every canonical document is reachable from [Documentation](../README.md).

`make docs-check` checks metadata, related IDs, links/anchors, reachability,
Task/maturity counts, topology, diagram and benchmark drift. Rendering tests
check projections. Human review checks comprehension and rhythm; no linter
claims to prove visual quality or runtime capability.
