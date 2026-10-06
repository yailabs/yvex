<!-- docs:metadata
title: Product management control-plane qualification
id: yvex.evaluation.product-management-control-plane
document: evaluation
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Product management control-plane qualification

[Evaluation](README.md) · [Public contract](../contracts/product-management.md) ·
[Lifecycle classification](../contracts/product-management-lifecycle-map.md)

## Qualified producer boundary

`YVEX.STUDIO.COMPUTE.CONTROL.PLANE.0` starts from published YVEX `803dd98d`.
The producer adds 36 capability-discovered product management operations over
authenticated HTTPS or a same-user public Unix socket; restricted SSH remains an
advanced transport with a distinct grant. Existing read-only
management v1 and separately granted finite computation retain their scopes.
The complete 184-operation CLI catalog has an explicit ordinary-product,
advanced-engineering, CLI-interaction or unsupported-remote disposition.

Protocol 25 carries a genuine Host lifetime nonce and Session lifetime identity.
Consequential runtime operations verify those identities at their native owner.
Management receipts retain request correlation, same-request recovery and
indeterminate outcomes; an acknowledgement lost by the client does not authorize
another mutation. Native Source acquisition retains its existing durable
operation/generation and supervisor rather than acquiring a second scheduler.

Model, Source, immutable Package, deployment profile, live Engine and
computational Session remain distinct. Profile registration/removal share one
native transaction lock; an expected Package identity is checked before removal.
Artifact support metadata is derived by its native owner. Transfer completion
is never promoted to verified payload, executable Package or Engine residency.

YAI Core is unchanged. These operations grant no YAI identity, Case authority,
provider trust, Participant role or entitlement. Direct Session generation is
outside a YAI Conversation. The independently owned Spark worktree is preserved.

## Evidence classes

Qualification was performed on Exon on 2026-10-06 using disposable configuration,
model, runtime and credential locations. The accepted source commit records the
implementation; this record does not make a dirty integration snapshot a release.
Raw local logs are under `/tmp/yvex-control-plane` and are not required artifacts
in Git. No operator Case was changed to obtain a result.

| Lane | Actual input and owner | Observed result | Exact claim |
| --- | --- | --- | --- |
| Source/model management | `tests/integration/product_management_models.py`; local forced-protocol harness, real domain owners and controlled HF provider | PASS: submit/observe/cancel/resume/complete; duplicate request retains receipt; stale Source generation refuses; source verification rejects unverified fixture headers; Build planning and missing-input refusal | Contract/domain qualification. Not SSH, live registry download or real-weight compilation. |
| Profiles and cleanup | Same protocol test; real tiny artifact and native registry/filesystem owners | PASS: four concurrent profile creations survive; native verification and bounded scan; stale removal refuses; exact removal retains Package bytes; generation-fenced cleanup dry run and explicit logs-only cleanup | Native metadata, transaction and preservation evidence. No executable readiness is inferred from an inspection profile. |
| Credentials and malformed input | Empty host credential state plus explicit provider fixture | PASS: opaque reference delegates to host Account owner; missing credential refuses; token/path/unknown input fields reject before receipt persistence | No new vault, frontend credential store or token-provisioning operation is claimed. |
| Native Session computation | `tests/integration/product_management_sessions.py`; compiled deterministic tiny CPU artifact and native Host/Engine | PASS: load/create/generate/fork/reset/close/unload; lost acknowledgement causes exactly one committed turn; recreated Session rejects stale lifetime mutations; restarted Host rejects old identity | Real native computation over controlled local management protocol. Not a real-model quality or physical-capacity claim. |
| Restricted SSH/public SDK computation | Optional `ssh-sdk` mode of `product_management_sessions.py`; actual isolated OpenSSH, public SDK and compiled tiny CPU owner | PASS: typed Engine/Session/generation results; lost response recovers the same receipt and exactly one turn; fork/reset/lifetime fences/unload/Host restart; idle cancellation reports its true failed posture | Complete public transport/client/native computational chain at synthetic tiny-model scope. Declared admission capacity is not real memory evidence. |
| Restricted SSH/public SDK | `tests/integration/product_management_runtime.py`; real isolated OpenSSH, public SDK client, native zero-engine Host | PASS: 10 controls covering explicit grants, read-only grant separation, empty inventory, live/restarted Host identity, stale mutation with zero engine effects, exact receipt recovery, redacted bounded job list and peer revocation | Actual SSH/SDK/producer composition. No resident model, Spark or human acceptance is implied. |
| Sibling CLI regressions | Native CLI preparation/profile contracts and `tests/source_acquisition_lifecycle.sh` | PASS: 14 preparation contracts, 16 profile contracts, supervised acquisition lifecycle | Shared owners preserve CLI behavior. No parallel CLI parser is used by management. |
| Focused producer inputs | Four `management_models` Rust unit tests | PASS: unknown secret/path fields, exact immutable revision requirement, bounded input, opaque host credential reference | Refusal occurs before owner mutation. |
| Catalog/schema parity | `tests/product_management_contract.py` and public YVEX SDK parity lane | PASS: 184 CLI dispositions, 36 product management operations, existing two v1 reads and separately granted finite computation; negative drift controls refuse | Typed producer/consumer contract coverage, not service availability. |

The tiny Session test explicitly declares `YVEX_TEST_FIXTURE_CAPACITY=1` and
uses the existing 128 GiB synthetic admission fixture. It refuses preexisting
conflicting test-capacity variables. The model executes on the real CPU; the
capacity declaration is not evidence of Exon's available physical memory. The actual SSH Session
command adds `YVEX_SESSION_MANAGEMENT_TRANSPORT=ssh-sdk` and an explicit public
SDK example executable. Its raw log is `sessions-sdk.log`; local protocol
evidence remains separately in `sessions-integration.log`.

The QA registry registers models, local Sessions, independently required SSH/SDK
Sessions, actual SSH runtime and operation-map checks. The Session lane builds `build/tests/tiny_compile` explicitly. Both SSH
lanes require `YVEX_SDK_PRODUCT_EXAMPLE`; a missing asset reports BLOCKED, never
PASS or an implicit skip. The registry now contains 187 tests, including separately asset-gated HTTPS/SDK
Session computation and the producer TLS/UDS security lane.

## Standalone network qualification

`network_management.py` passes with generated loopback TLS identities and a
private same-user socket. Controls cover anonymous identity-only observation,
closed/pending/approved pairing, wrong credentials, repeated/ambiguous HTTP
headers and Origin refusal, 36 shared operations, transport-separated receipt
scope, restart persistence, remote revocation without local data loss, eight
bounded connections, absolute TLS read deadline and malformed private storage.
The local identity precondition is checked before any operation dispatch.
Expired/revoked request identities cannot revive; retry requires a new client
credential. Six local service/pairing CLI operations bring the classified
operator inventory to 184; the public product API stays at 36.

The SDK-owned native network lane uses the actual desktop SecretService and
fresh client processes, then forgets its generated credential. HTTPS Session
mode in `product_management_sessions.py` passes with the real public SDK,
protected native client credential and compiled tiny CPU model: load,
generation, fork/reset/close/unload, lost acknowledgement with one committed
turn, exact receipt recovery and stale Session/Host refusal. Idle cancellation
is a truthful failed operation; active cancellation is qualified by native
server tests, not that one-token fixture. This is not real-model quality or
Spark acceptance. No operator credentials, Cases or active runtime were touched.

RUSTUP_TOOLCHAIN 1.98.1 builds the producer and passes 16 management unit tests
and five Rust structural tests. Its Clippy component is unavailable. Installed
stable Clippy 1.93.1 with `--ignore-rust-version` reports one pre-existing
`nonminimal_bool` expression in `src/cli/rust/attention.rs` at line 266; all new-owner
warnings were repaired. This is not reported as a green aggregate lint gate.
Documentation, QA, source ownership and repository layout gates pass.

## Native desktop to isolated Spark HTTPS

Published producer `6e6e8f9c4372e7ed3446402df344bf301ab44752` was built for
Linux aarch64 in a private temporary Spark checkout, using two CPU build jobs.
Spark could not resolve GitHub, so the exact published Git bundle and verified
public dependency sources were transferred for an offline build. Source remained
clean. The binary SHA-256 is
`9dccdffdb6ef1d8e9a8eda0ea9d13043b2da938ff26ca1e63a60f91263edde8a`.
No operator installation, dirty source checkout, active Host or model was replaced.

The actual Tauri Studio client reached that isolated service over HTTPS through
Tailscale. Public certificate identity was compared with the server-local value;
a generated protected native credential entered pending state and its exact
request was approved locally. Studio automatically connected, discovered 36
operations, read the zero-engine Host and tiny model profile, then authored
Engine load, Session creation and direct generation through the public SDK.
All three durable jobs succeeded: one generated token, one committed turn.
Studio restart restored the protected connection. The isolated Case generation
stayed 3 before and after; no operator Case was involved.

The tiny model is a deterministic compiled CPU fixture. Spark admission consumed
actual physical capacity observations, with no synthetic capacity override.
This proves real cross-machine transport and native client/computation composition;
it is not real-model quality, CUDA qualification or human visual acceptance.
Tailscale reachability is not a physical-LAN mDNS acceptance claim. SSH was used
only for isolated deployment and local approval, never by Studio's client transport.

The first attempt truthfully refused because the disposable environment lacked
HOME; it was repaired with a private test home before the successful run. Both
generated grants were revoked, the client removed its native credential and only
the verified temporary service/Host processes were stopped. Public source/build
and cleanup evidence is retained locally under `spark-https`; Studio's native
result and screenshots are under `/tmp/studio-native-yvex-spark`.

## Operator Spark LAN rollout

On 2026-10-06, operator-authorized deployment installed clean published source
`c8e7bc9e8ad41fb71a2fa047c2ec4e5a11c21e5f`, tree
`a9e8d5d13461a707bb92fb99297a50d0ab7104a4`, using the supported `make install`
path in a separate checkout. Its executable SHA-256 is
`487944b1039f780e572cfa90e8272e9c330e180b245179b3dfffc29e41221710`.
The unpublished CUDA/qualification candidate was not deployed. The independent
user supervisor `yvex-management.service` starts HTTPS and mDNS without a
dependency that restarts or replaces `yvex-host.service`.

The public operator handoff is retained outside Git at
`/home/dgmothx/lab/models/evidence/yvex-management-rollout-20261006/handoff.json`;
the read-only reproduction probe and receipt share that directory. The handoff
owns deployed endpoint, certificate fingerprint and observed Engine identity.
Addresses are operational observations, not permanent product identities.

| Control | Expected | Observed | Claim |
| --- | --- | --- | --- |
| Installed software | Clean published source, matching build/install executable | PASS; installed version reports source clean, exact commit/tree and protocol 25 | Identity-bound management software; not release readiness |
| Physical-LAN HTTPS, Exon → DGX | Certificate pin checked before HTTP; public identity only | PASS; `/v1/identity` returns 200 with the exact server-local TLS identity | Deployed LAN reachability, not enrollment |
| Physical-LAN discovery | Resolved `_yvex-management._tcp.local.` announcement during a five-second Exon observation | PASS; address/port, service name, protocol and fingerprint resolve on Exon's Wi-Fi interface | Real LAN discovery; not authorization or global discovery |
| Anonymous management | Refuse inventory/operation dispatch | PASS; 403 `credential_required` | Authentication fence retained |
| Same-user public management | Discover 36 operations; read native catalog | PASS; protocol 25, catalog readable with runtime observation explicitly unavailable | Public operation inventory and local catalog, not live Engine visibility |
| Old Host compatibility | Refuse protocol mismatch without mutation or fabricated lifetime identity | PASS; `host.get` reports unavailable; `engine.list` unavailable | Fail-closed compatibility, not a stopped Host |
| Operator runtime preservation | Same process, executable, Engine and generation; no inference dispatch | PASS; existing protocol-24 Host and DeepSeek generation 1 remain active; process restart count stays zero | No operator runtime/model replacement or Case retry |
| Isolated TLS/UDS controls on installed binary | Existing 12-control generated-identity suite passes | PASS; `network_management.py` including restart, revocation, framing, deadline and unsafe-storage refusal | Software/security regression, distinct from operator enrollment |

The listener is now operational for discovery/identity and local catalog reads.
Pairing remains closed by default; no new client was enrolled or automatically
trusted. The operator explicitly approved the new TLS pin, separately from the
previously approved SSH identity. Actual Studio enrollment and its real native
consumption remain consumer evidence.

The preserved operator Host still runs `803dd98d` with private protocol 24.
Management requires protocol 25's genuine Host/Session lifetime fences, so this
rollout does **not** qualify administration of that resident DeepSeek Engine.
A separately coordinated Host upgrade is required; no compatibility shim,
fake nonce, old CLI parsing or implicit restart was introduced. The Host's
loopback OpenAI listener remains distinct from management HTTPS. No finite-decision
Engine is resident, and management enrollment grants no finite computation.

## Real anonymous registry observation

The public management `model.search` and `model.inspect` operations were exercised
against the actual Hugging Face service with an isolated empty `HF_HOME`, token
environment variables removed and implicit token use disabled. No password,
existing credential file, weight download or registry mutation was used.

The canonical publisher search returned `Qwen/Qwen3-0.6B`. Exact inspection
resolved and retained revision
`c1899de289a04d12100db370d81485cdf75e47ca`, with one published representation.
The model root remained empty. A separate real search result,
`litert-community/Qwen3-0.6B`, honestly returned zero supported representations;
its presence did not manufacture acquisition or runtime support.

Result: PASS for real anonymous registry metadata through the public producer
contract. The transport harness in this lane is local; actual SSH transport is
qualified independently. Private/gated registry authorization, large acquisition,
real-weight compilation and model quality are not established by these reads.

## Aggregate gate and environmental limits

The requested CPU command is:

```sh
RUSTUP_TOOLCHAIN=1.98.1 make check NVCC_AVAILABLE=no
```

Independent remaining targets were also collected with Make's keep-going option;
this does not turn the aggregate result into PASS. The final keep-going aggregate exits 2, with exactly these failed targets:

- `test-core`: `cuda.runtime_binding` requires a generated native CUDA bundle;
  the deliberate no-NVCC build has none. No GPU rebuild, weakened assertion or
  implicit skip was introduced to hide this refusal.
- `test-tiny-vertical`: native model admission cannot preserve the required
  system reserve after residency on the currently available memory. The aggregate
  was not rerun under a synthetic capacity override. The explicitly declared
  isolated Session fixture above retains its narrower positive result.

Documentation architecture, QA registry/properties, source/FFI ownership,
repository layout, architecture boundaries and complete native CLI/cutover
checks pass in the collected independent lanes. The final aggregate log remains
`check.log`; unavailable GPU and physical-capacity evidence are not promoted by
passing management/CPU controls.

## Remaining product boundaries

Host provider credential provisioning remains explicit at the existing Account
owner. `registry:huggingface:default` is a reference to that owner's configured
credential, not a token, new credential database or browser authorization flow.
Ordinary acquisition/build/runtime/session operations are public; arbitrary local
file import/export, low-level compiler recipes and checkpoint-file movement need
bounded transfer/plan identities before remote promotion. The classification
map records these obligations instead of asserting command-name parity.

Training remains unadvertised. Dataset, recipe, Training run, checkpoint/adapter,
evaluation, promotion and their provenance/recovery require producer semantic
owners. Existing Model/Package/Job identities provide composition seams, not
fabricated training support.

Operator HTTPS/discovery rollout is qualified at the bounded scope above. Actual
Studio enrollment/visual acceptance, protocol-compatible operator runtime
administration, real-model management mutations, long-lived reliability and full
commercial/release qualification retain their own consumer/environment evidence.
This producer record alone does not close those boundaries.
