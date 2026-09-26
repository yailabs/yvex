# YVEX agent rules

YVEX is a native C/CUDA compiler and runtime for identity-bound verified
open-weight inference. Code and tests own capability; documentation describes
implemented truth. [ROADMAP.md](ROADMAP.md) is the sole live macro
project-control authority. Consult the [engineering method](docs/development/agentic-engineering.md)
when framing or closing a substantial delivery, and the [documentation map](docs/README.md)
for task-relevant architecture and contracts; neither is a mandatory pre-read
for a small local change.

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
headers, or flattened object identities. See [source ownership](docs/development/source-ownership.md)
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
launch geometry and device profiling. CUDA details stay below that boundary.
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

Use [QA ownership](docs/development/qa.md) and its registered change mapping
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
