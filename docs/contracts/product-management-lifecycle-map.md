<!-- docs:metadata
title: Product management lifecycle classification
id: yvex.contracts.product-management-lifecycle-map
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Product management lifecycle classification

[Up](README.md)

This map classifies the source, model, package and deployment-profile CLI owners
against the product-management contract. Command spelling is an entry point;
shared native/domain operations remain the owner. A protocol receipt means an
operation was accepted or completed at its reported boundary. It never upgrades
transfer completion to verified payload, package readiness or Engine residency.

## Ordinary product lifecycle

| CLI owner | Public management consumer | Boundary |
| --- | --- | --- |
| `model.search` repository search/inspection | `model.search`, `model.inspect` | Native catalog provider adapter; exact resolved revision and representation identities retained. |
| `model.list`, `model.show` | `model.list`, `model.get` | Native Library joins Sources, Packages, deployment profiles and exact observed Engines. Runtime observation failure is separate. |
| `source.list`, `source.show`, `artifact.list`, `profile.list`, `profile.show` | `model.list`, `model.get`, `package.get` | Typed domain projections within the model record; no parallel inventory or invented lineage. |
| Remote-source branch of `model.pull`, `source.acquire` | `acquisition.start` | An immutable repository revision and producer representation select the existing detached Source operation. |
| `model.acquisition.status`, `source.status` | `acquisition.get` | Source operation identity, generation, transfer lifecycle and nullable progress retain their owner meanings. |
| `model.acquisition.stop`, `source.stop` | `acquisition.cancel` | Exact operation/generation fence; native process ownership and cooperative stop. |
| `source.resume` | `acquisition.resume` | Exact previous operation/generation under the Source lock; retained bytes and selection, new generation only when admitted. |
| `source.cleanup` | `source.cleanup` | Shared guarded cleanup with exact generation and selected scope. Explicit confirmation; active-owner/process/path safeguards remain. |
| `model.prepare` | `build.start` | The same Preparation/Variant/binding/profile composition; background management receipt, no second compiler. Dry run is planning evidence. |
| `source.payload.verify` | `source.verify` | Native exact repository/revision target catalog resolves verifier identity. Unsupported target/revision remains a refusal. |
| `artifact.verify`, integrity portion of `artifact.verify.report` | `package.verify` | Exact local package digest; native integrity report. Integrity does not prove executable model support. |
| `model.storage` | `model.storage` | Native inode/accounting facts, shared cache scope and unknown reflink/history remain explicit. |
| `model.evict` | `model.evict` | Exact source/package identity and confirmation; native recoverability and active-use checks, retained remote provenance. |
| `profile.create` | `profile.create` | Register a known local Package, or derive a named deployment from an existing native profile. Backend/binding/target are owner-derived; native admission validates context. |
| `profile.remove` | `profile.remove` | Expected immutable Package identity is compared under the same native registry transaction lock used by creation, before removal/save. Package bytes remain. |
| `profile.scan` | `profile.scan` | Background scan of the configured model root; bounded candidate projection. Candidates are not admitted deployments. |
| `profile.verify` | `profile.verify` | Native identity/metadata/readiness observations remain distinct. |

`build.start` accepts source model identity and optional qualified representation
selection (`quant`). The native preparation owner selects the currently admitted
target/backend. No client chooses hardware support from a product name. A general
cross-hardware build planner is not currently exposed by this workflow.

## Local filesystem interaction and producer provisioning

| CLI owner | Classification and exact reason |
| --- | --- |
| Local file/directory branch of `model.pull` | Local filesystem import. A remote management grant is not authority to read any arbitrary host path. A future remote import needs a bounded transfer/staging identity and no-follow admission; sending a path string is not that contract. Remote registry acquisition is available now. |
| `model.push` | Immutable local/file export. The live CLI explicitly refuses non-file URI schemes; it is not registry upload. Export to the consumer requires a transfer/download owner and a user-selected destination on that consumer, not an arbitrary remote write path. No remote-publishing capability is advertised. |
| `source.manifest` | Advanced producer artifact authoring with explicit source/output files. Ordinary acquisition already produces owner-authored provenance. A management client cannot manufacture verified Source status by uploading a manifest claim. |
| Account login/logout and credential provisioning | Producer-host provider credential owner. `registry.accounts` exposes safe availability and `registry:huggingface:default` only when the existing native Account observer reports authenticated credentials. Acquisition accepts the reference, never a token, token path or credential body. Provisioning remains explicit on the host through the current provider owner. No new vault or browser authorization service is claimed. |
| Low-level checkpoint/file export/import | Explicit local serialization destination/source interaction. Public Session fork and exact state observations are separate from moving a checkpoint file. Remote checkpoint transfer requires immutable transfer identity, destination policy and current Session state admission; no arbitrary server path API is introduced. |

A management client with a credential reference cannot provision, replace or
extract a credential. Missing host authentication returns
`registry_authentication_required_on_host`; it never attempts interactive login
inside the remote worker. Anonymous and configured authentication are distinct.

## Advanced engineering and inspection

`artifact.materialize`, `artifact.prepare` (`compile`), controlled artifact emit,
templates, quantization jobs, physical plans, materialization/model gates, tensor
maps, native tensor enumeration and detailed target/compiler diagnostic commands
remain advanced engineering owners. Ordinary model preparation is available via
`build.start`; these commands additionally accept explicit physical plan files,
policy inputs, compiler experiment controls or arbitrary output destinations.
They are not alternate product acquisition/build implementations hidden from the
SDK. Promoting one requires an exact typed artifact/plan identity and its own
admission obligations, not a command-name proxy.

`source.inspect`, detailed artifact metadata/registry/tensor inspection and
`artifact.check` expose deeper evidence than the current model/package summary.
Summary identity, current integrity and readiness are projected today. Large
paged tensor/compiler evidence is an explicit remaining inspection-depth
extension; management does not pretend that summary fields contain it.

Removed registry paths remain removed. Legacy CLI aliases, TTY selection,
progress rendering, colors, JSON/audit switches and completion are CLI
interaction and do not create management operations.

## Recovery and evidence

Management mutations have durable request receipts. Reusing a request identity
with the same operation/input observes the same receipt; changed input refuses.
A lost worker is indeterminate and is observed, never blindly redispatched.
Acquisition additionally exposes its existing native Source operation, whose
lifetime is independent of Studio and the management request process.

`tests/integration/product_management_models.py` exercises disposable trust and
forced-protocol/domain handling with a controlled provider fixture. Its claim is
not real SSH transport, Hugging Face service compatibility, real-model compiler
execution or human Studio acceptance. Native CLI preparation, profile and Source
lifecycle tests remain sibling-client regression evidence.

## Training extension

Training is not an advertised management capability. The selected model/library
identities distinguish Source, immutable Package, deployment profile and live
Engine; a derived model can enter that same lifecycle. Future training requires
producer-owned Dataset, recipe, Training run, checkpoint/adapter, evaluation and
promotion contracts, exact lineage, cancellation/recovery and qualified execution
before SDK types or Studio production navigation can advertise them. The existing
job receipt mechanism does not supply those missing semantics.
