# Model storage contract

The source catalog owns provider origin and acquisition facts. The artifact
registry owns representations and optional deployment profiles. The model
library joins those facts into logical models; filesystem proximity never
establishes lineage. These owners remain authoritative for remote-only records,
managed local material and explicit external references.

![Durable source and artifact retention, logical working-set membership, READY deployments, loaded engines and active work occupy separate authority domains.](../diagrams/storage_residency.svg)

*Figure 6 — Storage versus runtime truth. Working-set membership is orthogonal
to retention and residency; explicit requests cross the boundaries. Cache is
not provenance, a file is not a deployment, and device-addressability does not
prove physical UMA page residency. No automatic eviction policy is implied.*
[Editable source](../diagrams/storage_residency.json).

## What users choose and YVEX manages

Users choose a model or an external path and, optionally, a model-storage root.
YVEX creates the required internal paths during acquisition and preparation.
Users do not need to build the hierarchy below, choose a tensor-plan directory,
or move downloaded files into an internal representation directory themselves.
The root is configurable; `--models-root /srv/yvex/models` overrides it for an
operation. Use the same root for subsequent operations or configure the default.

The standard top-level layout is:

```text
<model-root>/
├── source/
├── representations/
├── registry/
├── evidence/
├── cache/
├── tmp/
├── inbox/
└── quarantine/
```

This is an ownership layout, not a requirement to pre-create every directory.
`model pull` places managed acquisitions and their provider cache/records;
`model prepare` selects derived output, plan and binding locations. Evidence
comes from the operation that produces it. Retained historical evidence and
quarantine are explicit operator dispositions, not automatic cleanup jobs.
Working-set membership belongs to the catalog, not an `active/` directory.

## Path ownership

The configured root has these durable projections:

| Location relative to the model root | Owner and meaning |
| --- | --- |
| `source/hf/<org>/<repo>/<revision>/` | Retained complete provider source at an immutable commit |
| `source/hf/<org>/<repo>/<revision>-<selection>/` | An explicitly selected immutable payload set, independent of other representations |
| `source/github/<org>/<repo>/<release>/` | Release acquisition; a release name alone does not prove immutable bytes |
| `source/local/<sha256>/` | Managed local adoption keyed by the complete representation digest |
| `representations/` | Derived representations; catalog records identify retained historical layouts |
| `registry/` | Existing acquisition records, provenance and immutable runtime bindings |
| `evidence/fixtures/` | Selected component proofs, separate from full model residency |
| `evidence/build/` | Preparation and acquisition evidence |
| `evidence/build/<family>/*.acquisition.operation.json` | Atomic supervised-acquisition operation state, never source provenance |
| `evidence/build/acquisition/` | Provider streams and supervisor audit records for acquisition operations |
| `evidence/calibration/` | Retained quantization inputs |
| `cache/hf/<org>/<repo>/<revision>[-<selection>]/` | Provider download state, linked by the corresponding source `.cache` |
| `cache/provider/huggingface/{hub,xet}/` | YVEX-selected provider caches; advanced environment overrides remain explicit |
| `cache/` | Other provider and runtime caches |
| `tmp/imports/`, `tmp/prepare/`, `tmp/acquisitions/` | In-progress owned operations and identity-scoped leases |
| `inbox/` | Optional intake surface, with no automatic catalog admission |
| `quarantine/` | Retained material with an explicitly unresolved disposition |

New preparation selects
`representations/<runtime-target>/<physical-variant-identity>/model.gguf` after
sealing the physical plan. The adjacent `physical.plan` preserves that plan.
An existing conflicting plan is rejected. A representation is not executable
until the artifact and deployment owners admit its exact binding.

The physical-variant directory identifies the sealed plan; its name is not the
full-file GGUF SHA-256. Exact output-byte identity is recorded separately.
An existing machine may also contain migrated paths such as
`representations/deepseek/candidates/`, older family-level GGUF files or
`*-bootstrap-*` files. These are cataloged historical material, not templates
for new preparation and not proof of another logical model. In particular,
historical `deepseek`/`deepseek4` or `qwen`/`qwen3_5` directory spellings do not
create independent model identities. Later cleanup requires an explicit
consumer and provenance review; normal downloads do not rearrange that history.

`registry/` under the model root contains acquisition, provenance and binding
material. The configured artifact registry remains the catalog authority;
directory scans do not create a second registry or establish lineage.

## Supervised acquisition operations

A provider transfer is a durable source operation, not the lifetime of the
terminal that started it. `model pull` and the advanced `source acquire`
surface start or resume an identity-bound supervisor and then attach as a
client. The supervisor owns the provider process in a separate process session;
loss of the invoking CLI, terminal or SSH connection does not signal the
transfer. A later `model status`, `source status`, stop or resume invocation
reopens the same durable operation.

The durable `yvex.source.acquisition.operation.v1` record binds provider,
repository, immutable revision, selection identity, canonical source path,
operation generation and a random content-hashed operation identity. Supervisor
and provider process identities additionally bind boot ID and process start
ticks. PID and process-group values are diagnostics and control coordinates,
not operation identity; a reused PID cannot authenticate stop, resume or
health. Publication is an fsync-and-rename transaction in the evidence/control
domain. It is not a source manifest, verified-source record or registry.

Lifecycle and health are independent facts. Lifecycle is one of `starting`,
`downloading`, `retrying`, `finalizing`, `stopped`, `failed` or `complete`.
Health is `unknown`, `healthy`, `degraded`, `stalled` or `not-applicable`.
A live process proves only process liveness. Fresh selected-object or provider
activity can establish progress freshness; no observable admitted signal leaves
health unknown. A provider retry is degraded rather than stalled. An expired
bounded progress window is stalled, while a slow provider that continues to
advance remains healthy.

Progress facts keep their domains separate:

- `expected_bytes` is the selected payload size when the catalog knows it;
- `committed_bytes` counts regular bytes already visible in the canonical
  source directory;
- `provider_activity_bytes` and its rates describe process write activity and
  are diagnostic, not selected payload bytes;
- `inflight_selected_bytes` and selected-payload rates remain unknown unless a
  provider adapter can prove their selected, non-overlapping domain;
- selected/completed/incomplete files, shards and sidecars are independent
  nullable facts;
- provider partial and lock objects are provider internals, not incomplete
  selected-file counts;
- retry count and current object exist only when a stable machine provider
  event supplies them; the authenticated provider-start event may establish
  event freshness, but later process I/O updates progress freshness only and
  never masquerades as another provider event.

Unknown is serialized as JSON `null` and rendered as `unknown`, never as zero.
No percentage is calculated from committed bytes plus provider cache or process
I/O because those domains can overlap. Provider-oriented human logs are audit
evidence and are not parsed into canonical progress.

The machine status projection is `yvex.model.acquisition.status.v2`. Its
`active`, `bytes` and `partial_files` compatibility fields retain the exact v1
meanings: process liveness, committed canonical bytes and provider-internal
objects. New consumers use the typed lifecycle, health and nullable progress
fields. Existing `yvex.model.acquisition.status.v1` operations without a v2
operation record remain read-only: YVEX reports their legacy state but never
silently reparents or reinterprets a running provider. An operator may stop the
legacy process through its existing explicit identity-bound path and then
resume under supervision.

Reconciliation authenticates process identity before control. A disappeared
supervisor becomes stopped/interrupted/resumable; an authenticated orphan
provider must be stopped explicitly before resume. Stop publishes terminal
state only after the exact provider or supervisor is gone. Unauthenticated or
unowned locks remain refused. Completed selected bytes and safe provider state
are retained for exact immutable resume. Provider cache roots default beneath
the selected models root; `HF_HUB_CACHE` and `HF_XET_CACHE` remain explicit
advanced overrides, and high-performance Xet mode is not forced globally.

Transfer completion and source verification are separate authorities. The
supervisor may enter `complete` only after the normal download/finalization
path publishes the selected population. Source manifests, tensor inventories,
content verification and later artifact admission retain their existing owners
and refusals.

## Importing material obtained elsewhere

`model pull` remains the acquisition boundary for provider IDs and local paths.
Local material can be adopted as a managed copy or retained as an explicit
external reference. Users do not need to construct internal directories.
Managed copies use the full content digest and an owned temporary path before
publication; a conflicting temporary path is refused. Directory and file
imports establish content identity once and retain rebuildable snapshot receipts.
Unchanged repeated imports check metadata; changed snapshots invalidate reuse.
Managed copies attempt reflinks before copying and never depend on the provider
cache entry remaining present. External references retain external ownership.
Adapters remain subject to their owning model or project catalog; their mere
presence under a model root does not adopt them into YVEX.

For example, `yvex model pull /downloads/model.gguf --managed` verifies and
adopts a copy; `yvex model pull /downloads/model.gguf --reference` verifies and
records the external path while leaving the bytes there. Directories use the
same operation. `inbox/` is optional: placing a file there does not trigger a
watcher, import, preparation or automatic catalog admission. Invoke `model pull`
on that path explicitly. Complete commands belong to the
[operator runbook](../operator-runbook.md#discover-acquire-and-prepare-a-model).

## Location, working set and runtime state

Provider origin is independent of current location. An acquisition record can
remain remote-only after payload removal. Exact provider repository, immutable
revision and relevant filenames/checksums are required evidence for eviction;
a similarly named repository or a moving branch is insufficient. Downloading
again is not necessary to retain a remote catalog fact. HF authentication and
availability remain operational prerequisites when rehydration is requested.

Registry schema `yvex.models.local.v8` records working-set membership as logical
model IDs. It survives normal registry writes and does not manufacture a local
source, artifact, startup profile or runtime capability. Older registries remain
readable. CLI JSON schemas `yvex.model.list.v4` and `yvex.model.v4` expose
working-set membership, local artifact availability and checksum-bound published
locations; the persistent wire protocol is unchanged.
`model list --json` projects membership alongside existing location
and readiness facts. DeepSeek Flash and its DSpark source variant share one
logical model, while their revisions, representations and profiles remain
distinct. Hardware identity belongs to build and validation evidence unless
the admitted representation itself proves a hardware-specific requirement.

The source target catalog declares that logical relationship as immutable
metadata: canonical model identity, exact registry aliases, and pinned provider
revisions. The generic library consumes it without a family-specific branch.
Another provider or revision cannot inherit it through a matching repository
name or runtime target. Exact accepted model selectors remain names of the
aggregate; artifact and deployment identities remain subordinate and distinct.

Cache and temporary state are mutable and never prove model identity. Source
bytes at a pinned revision and published representations must not be silently
replaced. Existing provider cache links are accepted only when they point at
their corresponding configured cache root. Migration of an old cache directory
is an explicit operator action, not an acquisition side effect.

`pull` and `push` concern distribution; `load` and `unload` concern runtime
residency. Working-set policy does not override either boundary. Unique
unpublished derivatives and their unresolved inputs remain local until exact
reproducibility or independently verified remote availability is established.
Publication, hardware qualification and eviction policy are not implied by
storage adoption. Model payloads, local catalogs, caches and generated evidence
stay outside Git.

[Release evidence](model-release.md) projects these owners into exact publication
candidates. Storage membership and working-set policy do not grant release
readiness.

## Idempotency and removal

The [model lifecycle guide](../model-lifecycle.md) owns user commands and provider
interoperability examples. These are the implementation invariants:

- Ordinary exact pull hits use catalog identity plus a verification receipt bound
  to device, inode, size, nanosecond mtime and ctime. No full-payload hash or
  provider discovery is needed for an unchanged verified published artifact.
- Explicit file selection is part of acquisition identity. Different selections
  and immutable revisions retain separate records and cannot merge their bytes
  into an earlier source snapshot. `--refresh` requests provider resolution;
  it does not replace older immutable payloads.
- Acquisition stages bytes before verification. Per-identity advisory locks
  serialize competing transfers; child provider processes retain the lease if
  the parent exits. Incomplete state never promotes a complete local record.
- Preparation identity includes the sealed source payload, transform, policy,
  calibration, numeric contract versions and relevant backend contract.
  Existing output must match that plan and its exact current byte verification.
- Runtime artifact handles retain shared inode locks. Eviction obtains an
  exclusive lock, rechecks the verified filesystem snapshot, and removes only
  eligible managed payload bytes. Catalog and publication identities survive.
- `unload` changes runtime residency only. `evict` is a separate explicit command.
  External references and unique unpublished artifacts are not eligible for
  managed representation eviction. Repeating successful eviction is a no-op.

Verification receipts under `cache/verification/` and the runtime reopen cache
are disposable evidence, not another registry. A missing or stale receipt does
not prove integrity and must not silently inherit a previous qualification.
`model storage` performs explicit allocation inspection. Shared inode allocation
is counted once; provider-wide cache rows, reflink extent sharing and historical
peak usage must not be presented as an exact model-specific reclaimable total.

Source eviction uses the existing immutable acquisition record and selected
inventory digest. The source owner holds an exclusive directory lease and
exclusive file pins, detaches the source into a private `tmp/evictions/`
operation directory, then removes it without following symbolic links. Source
payload sessions hold shared directory leases. A detached incomplete cleanup
never appears as a complete acquisition; its residual bytes remain explicit
temporary storage. Rehydration retains the original revision, selection and
expected source digest.
