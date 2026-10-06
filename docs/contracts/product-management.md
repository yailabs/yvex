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
Host process/service provisioning remains a local deployment responsibility;
management observes stopped Hosts rather than implicitly starting or replacing
an operator service. Engine load/unload never replaces the Host.

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
