<!-- docs:metadata
title: Product management v2
id: yvex.contracts.product-management
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Product management v2

[Up](README.md) · [Transport enrollment](remote-management.md)

YVEX exposes its Source, compilation, immutable package, deployment profile,
Host, Engine and computational Session owners to sibling CLI and SDK clients.
A YAI ProviderTarget remains a governed inference connection. This protocol
creates no YAI Principal, Participant, Case, binding, trust or entitlement.
Direct generation is computational testing outside a YAI Conversation.

## Transport and enrollment

Ordinary product clients use the standalone [native network service](network-management.md):
a protected same-user Unix socket locally or authenticated HTTPS remotely.
Optional mDNS discovers candidates; explicit certificate trust and local approval
are required for remote access. The service checks authorization and selected
identity before the shared operation dispatcher admits a request.

The existing restricted SSH substrate remains an advanced compatibility transport
carrying one bounded JSON request/response. Enrollment explicitly selects
`--scope product-management`; v1 read keys and finite-decision keys do not acquire
that grant. All transports preserve authenticated peer and device identity,
exact request correlation, capability discovery and the same receipt semantics.
Management service, inference address and computational Host instance are separate.

The request/response schemas are
[request v2](schemas/product-management-request.v2.schema.json) and
[response v2](schemas/product-management-response.v2.schema.json).
`management.capabilities` publishes exact operation IDs/kinds and bounds, not
marketing version guesses. Unknown operations and unsupported input refuse.
Training is absent from the operation catalog; clients must not advertise it.

Requests carry a 64-hex random `request_id`, `operation` and typed `input`.
The envelope is bounded to 128 KiB, responses to 1 MiB. Public credentials are
references, never tokens. A client must retain a mutation reference before
sending, and must not automatically retry after a transport failure.

## Objects and ordinary lifecycle

| Public area | Meaning and owner |
| --- | --- |
| `model.list/get` | Source/representation/package/profile catalog, not just served inference models |
| `model.search/inspect` | Provider-neutral remote discovery and exact repository revision/representation inspection |
| `registry.accounts` | Safe observation of the Host's existing registry credential owner |
| `acquisition.start/get/cancel/resume` | Existing Source delivery operation, identity and generation; resume is explicit and fenced |
| `build.start` | Shared preparation owner, verification and immutable output; dry-run plans never claim a built package |
| `optimization.techniques/plan` | Native technique inspection and bounded catalog-selected Program P planning Job; no artifact emission or Engine load |
| `qualification.get` | Exact embedded publication ID; full target, claim planes and measurements; no local receipt paths or promotion |
| `package.get/verify`, `source.verify` | Exact package/source identity and actual verifier, not inferred readiness |
| `profile.scan/create/verify/remove` | Deployment specialization separate from package and live Engine |
| `model.storage/evict`, `source.cleanup` | Owner-observed storage and explicitly confirmed/fenced lifecycle operations |
| `host.get`, `engine.list/load/unload` | Persistent zero-or-many-engine Host and exact Engine generation |
| `session.list/get/create/fork/close/reset` | Server-owned computational state, not a Conversation/Thread |
| `generation.start/cancel` | Explicit bounded testing, native cancellation and producer-authored public output channels |
| `job.list/get` | Exact peer-scoped submission receipts, including uncertain outcomes |
| `observe.events` | Bounded owner-authored runtime event stream with sequence cursor |

The complete operator classification is maintained alongside the public catalog.
Low-level tensor, compiler, quantization research and numerical probes remain
engineering tools. Local shell interaction and filesystem export/checkpoint
paths are not silently promoted to unrestricted remote file operations.
Host process configuration remains a local deployment responsibility. Explicitly
configured managed Hosts support separately granted lifecycle control below;
external operator services are never implicitly replaced. Engine load/unload
never replaces the Host.

## Physical compilation consumer handoff

Management v2 adds capability-discovered `optimization.techniques` and
`qualification.get` (reads), and `optimization.plan` (Job). Older producers lacking these operation IDs are
unsupported. These operations retain the existing product-management grant, transport trust
and peer-scoped Job store. Publishing the contract does not roll out a service.

| Stage | Consumer operation | Qualified boundary |
| --- | --- | --- |
| Select source | `model.list/get` | Acquired model and exact revision; presence is not payload authentication. |
| Inspect methods | `optimization.techniques` | `yvex.optimization.techniques.v1`: actual native producers and prerequisites, not research names. |
| Prepare intent | SDK `OptimizationPlanInput` / `operations::OptimizationPlan` | Catalog model/revision, backend, goal, context, prefill, concurrency and search bounds. No filesystem paths. |
| Submit once | `optimization.plan` | Persist invocation/request ID before dispatch. The same C planner as `compile optimize` owns candidate identities and costs. |
| Recover | `job.get` with the original request ID | Accepted/running/succeeded/failed/indeterminate remain distinct; loss is not permission to resubmit with another ID. |
| Inspect result | `yvex.optimization.plan.v1` | Semantic context, physical candidates, build identity, initial resource bounds and missing evidence; no fabricated qualified winner. |
| Read related evidence | `qualification.get {"id":"exact-publication-id"}` | `yvex.qualification.record.v1` carries the full canonical receipt, content identity and publication authentication. Missing IDs and paths refuse; a read performs no experiment. |
| Produce/evaluate | Local `compile optimize --select --out-policy`, `compile quant plan/emit`, qualification | Search-row production/evaluation remains local. `build.start` is a separate admitted preparation workflow, not arbitrary candidate execution. |
| Deploy | Explicit profile/Engine operations | Actual binding, resource admission and operator intent are still required. Successful planning starts nothing. |

Example input for an acquired catalog model:

```json
{
  "model": "mamba-codestral-7b-v0.1",
  "expected_revision": "4f086c08c1e0f07bdc50ca25125dbbf7475d21da",
  "backend": "cpu", "goal": "memory",
  "context": 32, "prefill": 4, "concurrency": 1,
  "allow_approximation": true
}
```

Context/prefill must be positive; prefill cannot exceed context. Concurrency and
candidate population are bounded to 32. Optional `memory_limit` cannot weaken
the native reserve. `technique` is `fixed-recipes-v1` or
`source-retention-allocation-v1`; allocation requires `weight_budget` and permits
at most 4096 `search_states` publicly. The expert local limit is separate.
Unknown fields, source/output paths and reserve overrides refuse before Job
persistence. Revision drift fails before compilation. Missing authentication,
calibration or consumers retain native refusals rather than optimistic readiness.

`publication` reports no artifact, binding or Engine publication. Unavailable
quality, rate, workspace and state measurements are null. Static ordering is
experiment priority, not measured ranking. Available memory is an observation,
not immutable artifact identity or a reservation. There is no public planning
cancellation operation: stopping observation does not stop native work.
Interrupted workers retain uncertain outcomes instead of automatic replay.

`qualification_evidence` contains metadata references to relevant embedded
published records. `qualification.get` retrieves each full receipt separately,
without duplicating raw sample populations in the planning Job. Association
by policy and transformation is explicitly weaker than exact calibration,
artifact and binding correspondence. Each row preserves authentication,
association and claim planes; `selection_eligible` remains false. An empty
collection means no related published records, not zero measured degradation.
Caller-authored measurements do not enter this planning operation.

The canonical [SDK contract](../../sdk/rust/contract/management.json) generates
Rust and TypeScript records. Rust uses `client.optimizations().techniques(...)`
and `prepare_plan(...)`, followed by `execute`/`jobs().get` and
`client.qualifications().get(...)`. The
[independent example](../../sdk/rust/examples/optimization.rs) retains an
invocation, dispatches once through the public same-user companion, and reads
the typed Job. HTTPS uses the same operations over an already approved
connection; no enrollment or trust changes are part of planning. TypeScript
owns safe records, not browser credential transport. Pin SDK `0.2.0` to the
exact producer repository revision and discover operations before use.

The real Mamba SDK/CLI control qualifies authenticated-source planning and
recovery, not inference, model quality, arbitrary remote compilation or Studio.
Source, candidate, artifact, specialization and resident generation remain
separate objects. Consumers must retain unknown evidence and refused rows.

## Managed Host control

`host.control` observes ownership (`managed`, `external`, `unconfigured`), exact
Host instance, control revision, configured inference port, explicit service-control
grant and admitted start/stop/restart actions. `host.start`, `host.stop` and
`host.restart` are durable Jobs. They are offered only for a locally provisioned
managed Host. Existing `yvex serve` processes remain external; no observation,
pairing approval or management restart silently adopts or replaces them.

Provisioning is an explicit local command while the Host is stopped:
`yvex management host-configure 8001 [--state-dir DIRECTORY]`. This stores a
bounded server configuration with the existing native socket, one worker, native
maximum-Engine defaults and a loopback inference listener on the selected port.
It starts nothing. No public request accepts arbitrary paths, executables or
shell arguments. Local same-user clients may control this explicitly configured
Host. Remote peers additionally need a separately granted `service_control_grant`
owner action; previously approved peers default to no service-control grant.
Existing restricted SSH product enrollment does not confer it.

Start requires `expected_revision`. Stop/restart additionally require exact
`host_instance` and `acknowledge_disruption: true`. The producer serializes control,
retains the request identity, checks native instance ownership, and runs an internal
process owner calling the same public native Host APIs as the CLI. The native
socket lock prevents duplicate Hosts. Stop completion requires process/lock
retirement, not merely the native stop acknowledgement. A timeout, response loss
or worker loss remains indeterminate. Pending lifecycle intent prevents a fresh
blind start/restart; reobserve the exact Job and Host. After an uncertain start
with no remaining process, explicit local reprovisioning while stopped is the
recovery boundary, rather than automatic replay.

Restart creates a new Host instance with **zero loaded models and Sessions**.
There is no automatic model/session restoration. It does not restart the HTTPS
management listener. Management restart likewise does not stop its managed Host.
The installed operator Host remains external until an explicitly authorized
migration stops it and configures the managed owner; no adoption operation is
published.

`observe.events` exposes the same native retained event/log owner as `yvex host
logs`, at stage depth, with bounded pages and a Host-bound cursor. It is not a
system journal or management-process stderr stream. This delivery does not add
full/token trace or an unbounded watch subscription.

## Jobs, progress and recovery

Mutations validate their bounded inputs before journaling. The producer persists
an accepted receipt under its private, peer-scoped data directory, then a worker
from the same loaded executable calls the native owner. This worker is a product
entrypoint, not a CLI-output parser or a second computational scheduler.

`accepted`, `running`, `succeeded`, `failed`, `cancelled` and `indeterminate` are
distinct. The same request ID with identical operation/input returns its existing
receipt; different input under that ID refuses. A lost worker becomes
`indeterminate` and is never automatically redispatched. Explicit owner reads
are the recovery route. Successful acquisition submission means the native
Source operation was admitted; `acquisition.get` supplies continuing delivery
progress and its terminal state. Closing Studio does not cancel that operation.

Receipt publication uses owner-only files, locks and atomic synchronized rename.
At most 512 receipts are retained per peer; capacity exhaustion refuses new
admissions instead of deleting recovery identities. `job.list` is bounded and
excludes prompts/input/result bodies. `job.get` returns one authorized exact
receipt. Prompt/result material remains local to the YVEX owner and is never
written into a Case. UI recovery storage retains references only. There is no
implicit receipt purge or promise of unlimited retained history.

Native generation reports a completed result only after `TURN_COMPLETE`.
Output retention is bounded to 256 KiB and 8,192 requested tokens. Public channel
fragments are decoded across UTF-8 boundaries; incomplete or corrupt output
remains uncertain. Only producer-authored public reasoning, final text, tool and
error channels are exposed. Transport completion is not generation completion.

## Lifetime and refusal fences

Native protocol 25 adds an optional exact Session lifetime fence. Current SDK
management requires it for destructive/session-generation actions. The native
owner compares it while holding its Session registry lock; a prior `SHOW` is not
an admission fence. Cancellation uses that same comparison. Closing and recreating
a name obtains a new identity. Reset retains the existing Session identity.
Media Sessions without a published lifetime identity refuse fenced actions.

The Host creates one random instance nonce for its lifetime and publishes it in
its authenticated native handshake. Management compares `host_instance` through
the same native connection used for dispatch. A restarted Host cannot accept an
old selection merely because its Engine counter starts again at one. The nonce
is not a machine, installation or commercial-device identity. It is not derived
from port numbers, paths, PIDs or timestamps.

Engine generations, exact package/profile provenance, source operation/generation
fences and existing native capacity/admission remain authoritative. A successful
management response grants no YAI Case authority. Unknown physical residency is
not zero; provider model visibility never proves residency.

## Registry credentials

Anonymous discovery/acquisition remains available where the registry permits it.
Private access uses the existing Host registry credential owner through an opaque
reference such as `registry:huggingface:default`. `registry.accounts` reports
whether that reference is available. Interactive provisioning stays with that
owner and is disclosed as `host_provider`; this contract does not accept raw
registry secrets in a job, React storage, argv, normal logs or Case history.
No web/commercial credential is reused as registry authority.

## Observation and future Training

Snapshots plus bounded cursor-based events recover current truth after reconnect.
Event retention/omission must remain visible; these are not an unlimited durable
log archive. Resource measurements retain their availability bits and exact
producer meaning, including mapped versus physically resident bytes.

Source identity, immutable packages, deployment profiles, jobs and capability
advertisement do not assume a remote registry is the only possible model origin.
Future Dataset/recipe/run/checkpoint/adapter/evaluation/promotion owners must
publish their own versioned lineage and operations. A derived model re-enters the
same Source/Build/Package/Runtime path. This contract reserves no fake Training
operations and permits no direct mining of YAI Case data.

## Platform convergence: observation and planning

Canonical public client: `sdk/rust` and `sdk/typescript`, independently owned by
YVEX. Read observations distinguish never observed, loading, current, stale,
unavailable, failed and unsupported. An absent value is not an empty collection.
Producer `host.get` may supply `last_known` with exact old Host instance, status and
observation time when its computational socket is unavailable. This bounded service
cache is observation only; Engine/Session mutations still require a current nonce.
A management-service restart may legitimately have no retained observation.

Runtime profile C schema v3 appends `readiness` and exact `compatibility`. The native
compatibility owner maps ready, blocked and incompatible outcomes; unknown remains
unknown. Existing JSON `launchable` and `blocker` remain compatible. Older producers
may omit the new fields, which never means an unlaunchable profile is available.

A dry-run `build.start` returns `plan_id`. A subsequent explicit build may provide
`expected_plan`; the shared preparation owner compares exact source/revision,
package/profile lineage and resolved plan under its existing source lease, before
compilation or publication. A changed plan fails with `build_plan_changed_review_again`.
This is a concurrency fence, not a reservation of memory or promised compilation.
Old unfenced clients retain their existing semantics; Studio's guided flow passes
the observed fence when the producer supplies it.

### Typed preparation assessment

`BuildResult.planning` optionally carries `yvex.build.planning.v1`. The same
preparation owner serves CLI and management. `readiness` uses ready, blocked,
incompatible, unavailable or unknown; native status codes determine classification,
never human message parsing. `basis` distinguishes admitted_recipe,
verified_profile and not_admitted. Exact reason/owner/status remain inspectable.

A known-model dry run can finish its observation Job with result state `BLOCKED`;
that is a completed assessment, not completed compilation. It has no `plan_id`.
An actual build under the same refusal still fails. Missing or ambiguous model
identity remains an operation failure. CLI dry-run reports the typed assessment
but retains a nonzero blocked exit. Older producers omit `planning`; clients must
not manufacture a readiness assessment from that absence.

Ready means the current recipe can be planned, or an existing local artifact was
verified through its current profile. It does not assert that a model fits the
connected Host. Runtime admission, quality and performance evidence remain
explicitly `not_evaluated`. Source verification, compile, package verification,
load and execution retain their existing independent gates. The assessment is
attached after sealing the established plan identity and cannot alter its meaning.

Current preparation selects the exact source's admitted recipe, quantization
policy/default and execution deployment. It does not implement generic hardware,
workload or quality-aware search. Connected management transport is not a target
hardware selection or an admitted distributed topology.
