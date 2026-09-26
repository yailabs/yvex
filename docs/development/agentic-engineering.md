# Agentic Engineering Method

The unit of agent-assisted engineering is a property to establish or falsify,
not a prescribed edit sequence. This method frames deliveries and reviews
claims; [AGENTS](../../AGENTS.md) owns mandatory repository invariants.

## Authorities

| Owner | Responsibility |
| --- | --- |
| [AGENTS](../../AGENTS.md) | Safety, source and execution boundaries, evidence honesty |
| [ROADMAP](../../ROADMAP.md) | The only live macro project-control surface |
| Code, schemas and tests | Implemented behavior and executable contracts |
| [QA](qa.md) | Test identities, change mapping, prerequisites and result states |
| Issues and pull requests | Bounded problem, delivery diff and evidence |
| ADRs and Git history | Durable decisions and retired chronology |

The [documentation map](../README.md) routes technical subjects to their
owners; it is not another status database.

## Frame the outcome

Prefer this order in a substantial task prompt:

1. **Outcome:** the observable after-state and the problem it resolves.
2. **Authority:** current code/evidence, consumer and relevant roadmap boundary.
3. **Freedom:** the implementation choices and safe local iteration delegated
   to the engineer.
4. **Invariants:** compatibility, ownership, safety and explicit non-goals.
5. **Completion:** falsifiers, verification surface, blocked stop condition and
   required handoff.

State the finish line without scripting every inspection, patch or test. A
capable engineer should discover the actual owner and proportional evidence.
For long-horizon work, say to continue through implementation, failed checks,
repair and revalidation until the defined result is earned or a real blocker
remains. A first working version is not the finish line. A thread-scoped Codex
Goal can preserve such an objective when explicitly requested; it does not
replace ROADMAP or grant broader authority.

## Investigate and implement

Reconcile live code and concurrent work before relying on a prompt's baseline.
Trace producer, consumer and mutable lifetime; a historical plan or report is
a lead to verify, not current architecture. Choose the smallest owner that
resolves the demonstrated property. A new registry, loader, runtime or policy
needs a real consumer, not a hypothetical future family.

Model/provider work uses immutable source and representation identity.
Qualification pressure should be real where the claim is real, and bounded
fixtures should discriminate failure modes. Use an independent numerical or
behavioral oracle when claiming conformance. Treat resource and hardware facts
as observations of the admitted deployment, not configured capacity.

## Classify evidence

![Evidence ladder from authenticated source to release, with explicit non-equivalences between inventory, component numerics, load, generation, quality and benchmark claims.](../diagrams/evidence_promotion.svg)

*Figure 7 — Each stronger claim needs its own evidence; this is not a test
schedule. Software QA cannot replace an independent numerical, behavior or
release gate.* [Editable source](../diagrams/evidence_promotion.json).

Before promoting a claim, ask what observation could disprove it. For
transactional work this often includes stale identity, cancellation,
rollback, cleanup and state isolation; for numerical work, independent
disagreement and non-finite inputs. Report the lowest claim supported by the
actual result. Missing backing stays missing, not an optimistic PASS.

## Verify the delivery

Review the final tree and evidence, not only the author report: changed owner,
affected consumers, refusal paths, registered test definitions, exact
artifact/binding or fixture, source stability and documentation. A push does
not itself verify remote identity; compare local and remote explicitly when
publication is claimed. Local runtime evidence may be recorded by identity
and pointer without implying that a remote reviewer reproduced it.

Use [QA ownership](qa.md) for the current mapped commands and prerequisites.
Run tests proportionate to the changed contract; repair failures caused by
the delivery and rerun against the final source state. Do not spend live-model
or GPU resources on a docs-only change.

## Decide progression

| Decision | Meaning |
| --- | --- |
| `proceed` | The implemented boundary is consumer-safe at its qualified scope |
| `repair_same_boundary` | A demonstrated semantic or implementation gap remains |
| `complete_evidence` | Implementation exists but required qualification is missing |
| `blocked_external` | A real external prerequisite prevents truthful closure |

`downstream_safe` describes only the exact earned consumer scope. Finishing a
prompt, passing a build, or selecting a roadmap row does not automatically
promote capability or start a successor. Reconcile any next pressure with
ROADMAP rather than manufacturing a series of micro-waves.

## Documentation lifecycle

Admit a document only for a distinct subject, audience, contract or lifecycle.
README is the public entry; runbooks own procedures; architecture explains
owners; contracts specify behavior; family records retain family-specific
evidence; release doctrine and records own their respective gates; ROADMAP
owns current macro state. Do not create a second status ledger or copy private
alignment rows into public documents. Retired audits, routine worklogs and
implementation chronology usually belong in Git history.

Code and generated authorities outrank prose copies. Link to the precise
owner instead of maintaining another table. CHANGELOG records externally
meaningful changes, not implementation diaries. Validate links, navigation
and affected surfaces through [CONTRIBUTING](../../CONTRIBUTING.md#tests) and
mapped QA.
