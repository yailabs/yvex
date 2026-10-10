<!-- docs:metadata
title: YVEX Agent Protocol
id: yvex.agents
document: guide
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Agent Protocol

**Task-based delivery with source, lifetime and evidence discipline.**

YVEX is a native compiler/runtime with a Rust product shell, a C computational
core, and CPU, CUDA and early Metal backends for identity-bound verified
open-weight inference. Backend presence does not establish model admission;
use exact [platform/backend evidence](docs/architecture/backend-execution.md).
Code/tests own capability; documentation describes it.
Use [Documentation](docs/README.md) for contextual routes, not universal preload.

## Start from a Task

For substantial work, select an already authorized Task or temporary Task Pack
from [TASKS](docs/project-control/TASKS.md). A newly authorized independent
outcome is recorded there before implementation. Do not select unrelated work.
Resolve outcome, affected plane, invariants, ABI, family, backend/hardware lane,
current [Status](docs/project-control/STATUS.md), required evidence and docs impact.
Small local fixes do not require reading every control or product document.

## Context routing

| Change | Minimum owner route, plus affected implementation/tests |
| --- | --- |
| Source / family | [Source](docs/architecture/source-provenance.md), [family integration](docs/model-families/integration.md), exact family record |
| Compiler / representation | [Compiler](docs/architecture/compiler-ir.md), [representation](docs/architecture/representation-artifacts.md), affected contract |
| Artifact / deployment | [Admission](docs/architecture/artifacts-admission.md), [deployment](docs/architecture/deployment-specialization.md) |
| Runtime / state / scheduling | [Runtime](docs/architecture/runtime-lifecycle.md), affected [plane](docs/architecture/README.md) and runtime contract |
| Backend / performance | [Backend](docs/architecture/backend-execution.md), [benchmark methodology](docs/evaluation/benchmarks/methodology.md) |
| Public integration | [Interfaces](docs/architecture/interfaces-protocols.md), exact [contract](docs/contracts/README.md) |
| Research / promotion | Relevant [research owner](docs/research/README.md), Status and independent evidence |

Read [Invariants](docs/architecture/INVARIANTS.md) when changing an ownership or
execution boundary. Read [ROADMAP](ROADMAP.md) only for long-horizon planning.
ADRs own structural selections; Memory is derived context.

## Shared development

The branch is a shared integration line, not an agent or family identity.
Before changing an affected owner, inspect the live branch, HEAD, staged and
unstaged work, and the region to edit; check remote relationship before
publishing. Preserve legitimate concurrent work, including compatible
same-file changes. Stage only owned
paths or hunks and review the staged diff. Do not reset or stash away unknown
work, rewrite published history, force-push, or resolve conflicts mechanically
with ours/theirs. Stop only at a genuine incompatible overlap or when new
authority is needed; integrate published histories with merge.

Authorized repository-local inspection, edits, disposable tests, repair and
reruns may proceed without repeated approval. Continue until the requested
property is verified or a real blocker is established; a first implementation
or green build alone is not completion. Protect exclusive resources such as
the GPU or a daemon port with their existing locks, and do not interrupt a
user-owned service merely to run a test.

## Source and execution ownership

Production is under `src/`, installed headers under `include/yvex/`, and tests
under `tests/`. `config/source_owners.tsv` is the sole production-membership
authority. Add a file only for a real ABI, lifecycle, reusable algorithm,
backend/platform or generated boundary, family recipe, or entrypoint; otherwise
extend its owner or keep a helper static. Paths form namespaces: lowercase
snake_case, no repeated tokens or `yvex_` source prefixes, root C/private
headers, or flattened object identities. See [source ownership](docs/guides/source-ownership.md)
for the change procedure.

Installed public headers live in `include/yvex/*.h`, cross-subsystem internal
headers in `include/yvex/internal/*.h`, and source-local shared headers in
`src/<subsystem>/private.h`. Public headers are C/C++ self-contained.
Production does not include `yvex/api.h`; internal headers do not include
source-private headers. Name dependencies explicitly; a non-public global
needs an internal ABI and multiple production consumers. Source files are at
most 2,000 physical lines, headers 600, functions 200; follow
`config/c_policy.json` without hiding size or warnings.

The dependency direction is core/public ABI → source/artifact/model/tokenizer →
compiler/graph → materialization/runtime → backend → generation → evaluation.
Domain facts flow through typed reports to renderers and CLI I/O. Production
never includes tests; lower layers do not depend on CLI. Generic owners do not
include family implementations. Planning does not depend on backend internals
or payload bytes; backends execute admitted operations without reconstructing
topology. Avoid include cycles and duplicate global symbols.

Source owns provenance and delivery; compilation owns semantic/physical
lowering and immutable bindings; artifact owns package admission and mapping;
deployment owns admitted implementation choices; engine generations own
executable resources and stale-reference boundaries; sessions own mutable
state; the scheduler owns ready progress; backends own device execution;
execution batches and expert worklists describe real selected populations;
evidence observes rather than controls it. Runtime consumes authenticated
bindings, not source inventories or family-name switches. Families own source
interpretation, tensor roles, schedules, state meaning, architecture-specific
operations and numerical obligations—not generic session or protocol policy.

Upstream declares legal work, numerical class and real populations; deployment
selects an admitted class; backends own buffers, submission, synchronization,
launch geometry and device profiling. Device-specific details stay below that boundary.
Optional acceleration may fall back only to a known-correct admitted path;
integrity failures, missing mandatory semantics and unsupported exact requests
fail closed.

Each session owns independent state. Transactional participants stage and
publish atomically or abort; KV, recurrent, draft, media, RNG, token ledger
and decoder state share lifecycle coordination, not storage geometry. Hash or
persist only facts needed across lifetimes. Never hash object memory, padding,
pointers, local paths or timestamps; within an authenticated engine generation,
prefer compact handles with recoverable lineage.

## Public and product contracts

One public schema/version identity denotes one layout and semantic contract.
Audit changed installed records and reject stale layouts before reading newly
added fields; bump wire versions only for wire-contract changes. Public headers
expose durable concepts, not backend
internals or test machinery.

`yvex` is the single product executable. Its server can run with zero engines;
load/unload creates or retires engine generations without restarting transport,
and requests route by model and generation. Public strategy names describe
semantics, not an implementation. CLI consumes typed APIs: input adapters
parse, renderers format, and only CLI I/O/server entrypoints write operator
output. UIs do not parse human output or invent telemetry.

The product shell is Rust under `src/cli/rust/`; native computational owners do
not depend on it. The `ffi` module alone crosses native ownership. Cargo derives
bindings from actual C headers and embeds the generated operator registry;
neither hand-authored ABI layouts nor a legacy C dispatcher is an alternative.
`make lib` remains independent of Cargo; `make client` builds the Rust product.
The Metal implementation stays under backend ownership; Darwin portability,
Metal primitive qualification and full-model GPU execution are separate claims.

## Evidence and completion

Keep software tests, independent numerical conformance, runtime lifecycle
qualification, component benchmarks, model behavior and release evidence
distinct. Internal YVEX agreement is not an upstream oracle; a tensor proof
is not a package, materialization is not execution, and component timing is
not a model benchmark. Missing mandatory evidence is `BLOCKED` or `SKIP`,
never `PASS`. Performance claims bind exact source/tree, package/binding,
backend/device, workload, warm/cold state, samples, memory and source
stability; separate measured from derived facts. Judge performance candidates
by throughput, latency, memory, preparation cost
and numerical effect together.

Use [QA ownership](docs/evaluation/qa.md) and its registered change mapping
to choose proportional tests. Safe local tests may be run, repaired and rerun
without another approval; expensive live/model work is not required for docs
or cosmetic changes. Evidence from a moving source snapshot is invalid.
Check the final diff and tracked payloads; weights, generated packages,
runtime dumps, raw profiles, registries, credentials, dependencies and build
products stay out of Git. Commit focused semantic boundaries.

Close only when the requested invariant, affected consumers, refusal/cleanup
paths and required evidence are actually qualified. Report exact source/tree,
material ownership and compatibility changes, expected versus observed
evidence, blocked gates and non-claims. For material QA, use one concise
evidence table; do not substitute test totals for the supported claim.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |

End a milestone closure with `progression_decision` (`proceed`,
`repair_same_boundary`, `complete_evidence` or `blocked_external`) and
`downstream_safe` (`true` or `false`), scoped to the exact earned claim. A
failure caused by the change calls for
repair and revalidation; stop only for a genuine external blocker or a decision
outside the authorized boundary. Do not promote the next roadmap boundary
merely because this one is complete.

## Model, representation and performance qualification reports

This protocol supplements the generic evidence table for model, quantization,
backend/hardware qualification, inference performance and release-target
benchmarking. Report an auditable after-state, never an execution diary. Use
the following order; omit a section only when genuinely not applicable:

| Section | Required content |
| --- | --- |
| RESULT | COMPLETE, IN PROGRESS or BLOCKED; exact Task; one sentence stating the earned claim. These report labels do not replace controlled Task-row states. |
| QUALIFICATION TARGET | One identity/configuration table: family contract, model/checkpoint and upstream revision, tokenizer/conversation, Transformation IR, physical policy/quantization, artifact, binding, specialization, source/tree/build/executable, backend/kernel, hardware/device count/topology, driver/runtime, memory, context/prefill/sequence/concurrency geometry, strategy, reasoning, sampling, product transport and workload suite. Include unchanged dimensions; unknown is explicit. |
| EVIDENCE MATRIX | Separate family semantics, checkpoint reference, representation quality, backend execution, deployment performance and product path: authority/reference, status, evidence and exact claim earned. A backend result may reference the deployment record, but must not disappear into one aggregate PASS. |
| QUALITY MATRIX | One row per exact checkpoint-matched representation; only available, authoritative reference metrics and their comparison policy/tolerance. Missing logits, NLL/PPL, KL, probability deltas, top-token or continuation evidence is unavailable, never zero error. |
| WORKLOAD MATRIX | Same case identity across admitted reasoning modes and execution strategies. Cells distinguish QUALIFIED, CHARACTERIZED, BLOCKED, UNSUPPORTED and NOT APPLICABLE; unexecuted admitted cells remain UNQUALIFIED. |
| PERFORMANCE MATRIX | Load, newly executed prefill, server/client TTFT, sustained committed decode, complete request and memory. Separate controlled-engine, product-native and HTTP lanes, target-only/speculative modes and batch/concurrency. Include samples, median and dispersion where required. |
| COMPARISON | Baseline/candidate rows passing the metric's machine-checkable comparison key; explicitly declare changed experimental axes. Never average incompatible configurations. |
| REGRESSIONS / REJECTED CANDIDATES | Material failures, numerical/resource/preparation tradeoffs and why a candidate was rejected. |
| BLOCKERS / NON-CLAIMS | Exact missing prerequisites and claims not earned; software/fixture agreement is not independent model evidence. |
| DOCUMENTATION / PUBLICATION | Generated public target, family/checkpoint, representation, hardware/backend and workload/reasoning views; methodology, reproduction commands and user-visible claim state. |
| EVIDENCE / GIT | Receipt/raw-evidence locations, immutable provenance/source stability, commits/tree/build/live executable, remote relationship and worktree state. |
| progression_decision / downstream_safe | Existing controlled values, scoped to the exact claim and safe next boundary. |

For reasoning families, use source-authored policies. DeepSeek requires none,
high and maximum, separately with target-only and DSpark where admitted. Use
the same workload across modes when semantically valid; never substitute an
easier maximum-mode prompt, shorten its source instruction or suppress its
reasoning stream. Retain input/reused/new positions, prefill time/rate, first
reasoning time, reasoning count/rate, source-authored reasoning-to-final
transition, first final time, final count/rate, total time and committed rate.
An unreached transition is NOT MEASURED. Speculative evidence additionally
retains proposals, verification population, accepted/rejected/discarded tokens,
accepted-prefix statistics and draft/verification/commit economics. A single
"thinking speed" is not an adequate report.

A naked `20 tok/s` is not a publishable claim: expose its exact target, metric
definition, workload and statistics. Family evidence does not qualify another
checkpoint, quantization, backend or topology. Local receipts do not become
YVEX-published qualifications. Machine evidence is canonical; generate public
views from it, never maintain benchmark values independently in prose. Raw JSON
alone does not satisfy the product publication exit. See
[methodology](docs/evaluation/benchmarks/methodology.md).

Device locks are advisory, not proof of uncontended operator hardware. Retain
resource-observation scope and sample admission separately from target identity.
Exclude observed contended or unavailable intervals from comparable performance
series, preserving their raw evidence and reason. Sampled clear lists do not
prove an uninterrupted reservation; never retire an operator's work for a test.

## Documentation and Task closure

Native model adaptation/post-training is an adopted future YVEX computational
product target, not a current capability or selected delivery. Follow the
[canonical research doctrine](docs/research/model-adaptation.md#native-adaptation-and-post-training-horizon):
YAI selects/authorizes semantic training material; YVEX owns generic computational
adaptation; Studio presents/orchestrates. External trainers may bootstrap or
supply references. Do not implement training, invent a YAI-specific trainer or
silently turn Case continuity into learned weights without an authorized Task.
Frozen-base adapter streaming does not qualify full-parameter training; training
provenance/loss does not qualify a resulting model or replace Recall.

Update executable architecture in the same Task as its change. Route public
ABI/protocol changes to Contracts, family support to the family record and
Status, observations to Evaluation, selected delivery to Tasks, structural
choices to an ADR, and release changes to the release record. Rebuild generated
benchmark/publication views from their sources. Update Memory only for durable
re-entry traps; Roadmap only when a macro horizon changes.

Closure explicitly reports: code; architecture; Task/Status impact; structural
decisions; earned evidence; evaluation/benchmark observations; changed canonical
docs; cross-repository effects. “Documentation impact: none” requires a concrete
reason. COMPLETE means the actual exit is earned; unresolved architecture or
missing mandatory evidence remains PARTIAL/BLOCKED delivery, never stale docs
silently deferred. Task rows themselves use only the four controlled states.

For the full method see [task delivery](docs/guides/agentic-engineering.md).
Stop for incompatible foreign work, unresolved ownership/authority, unpublished
external contracts or evidence required for a claim that cannot be obtained.
Resolve routine implementation and editorial choices without repeated approval.

## Product experience handoff

For substantial product-facing contract changes, follow the local
[Studio consumer handoff](docs/architecture/interfaces-protocols.md#studio-consumer-handoff). Supply exact public
identities, revisions, availability, actions/recovery and reproducible observations.
Studio owns the [experience integration contract](https://github.com/yailabs/studio/blob/main/docs/interaction-contracts.md#product-experience-integration) and its existing Task's
navigation/rendering qualification. No new endpoint implies a new UI page, and
producer/SDK qualification does not grant Studio or human acceptance. Unaffected
changes need no Studio documentation preload.
