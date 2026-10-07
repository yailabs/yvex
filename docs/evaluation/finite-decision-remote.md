<!-- docs:metadata
title: Remote finite-decision producer qualification
id: yvex.evaluation.finite-decision-remote
document: evaluation
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Remote finite-decision producer qualification

**A public computation contract, not a remote model-quality or YAI claim.**

[Evaluation](README.md) · [Contract](../contracts/finite-decision-remote.md)

## Exact scope

`INTERFACES.FINITE.DECISION.REMOTE.PRODUCER.0` adds one separately versioned
JSONL producer over the existing restricted OpenSSH substrate. Rust projects
the installed C finite producer request/result; C retains private protocol
v24, model/input policy, execution and cleanup. Explicit compute enrollment
does not promote management v1 peers or permit remote lifecycle mutation.

Qualification used a focused source snapshot based on main
`d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9`, with no DeepSeek optimization
delta. The accepted Task commit identifies that exact producer delta.
Raw logs/build facts are retained outside Git at
`/home/dgmothx/lab/models/evidence/finite-remote-20261005.ZQ2LNw`;
the isolated short-path checkout is `/tmp/yvex-finite.nOZWEU/source`.
The source, immutable REPLAI receipt and native library were qualified separately
from the dirty operator checkout. No production service was replaced.

## Evidence

| Lane | Authority / input | Expected | Observed | Result / claim |
| --- | --- | --- | --- | --- |
| Remote producer | Real isolated OpenSSH, explicit compute key, public C client, canonical native codecs; synthetic two-candidate peer | Exact schema, pins/correlation, model lineage, generation, IDs and raw scores; no partial result on failure | 32 controls pass, including unknown/malformed/duplicate/oversized/NUL input, stale/foreign generation, foreign population, unknown key, wrong host pin, forbidden command, read-only key separation, revocation/scope replacement over multiplexed transport, loss/no-retry and independent recovery | PASS; bounded producer/transport contract only |
| Existing management | Independent two-peer restricted SSH fixture | Two unchanged read operations and refusal/revocation behavior | PASS with stopped/running isolated host and current trust recheck | PASS; no implicit compute grant |
| Rust shell | Native unit/structural, fmt, clippy, CLI contracts and real PTYs | Registry-derived dispatch, safe Rust outside FFI, JSON and terminal compatibility | 55 unit PASS, 1 existing ignored test; 5 structural PASS; clippy/fmt PASS; PTYs at 40/80/180 columns PASS | PASS; product shell, not inference quality |
| Native protocol/host | Existing canonical C protocol and server suites | Unchanged wire semantics, finite result refusal and lifecycle | Both suites PASS | PASS; no wire/installed ABI change |
| Public C API | Installed-record and C/C++ consumer checks | Existing 40 versioned records retain layout | All 40 declaration/layout controls PASS in C and C++ | PASS; unchanged installed ABI |
| Product regressions | Complete native CLI and deterministic tiny vertical | Existing commands, load/session/cancel/recovery semantics remain usable | CLI/cutover PASS; tiny artifact/binding/HTTP/native control PASS | PASS; bounded software fixture, not a real finite model |
| Structure/publication | Source membership, repository/architecture boundaries, registry and document checks | One owner, exact grammar, valid contracts and generated reader | PASS; HTML reader includes contract and machine schemas | PASS; no release claim |

The first CLI attempt under the long evidence-directory checkout exceeded the
Unix socket path bound. An absolute fixture-root attempt also exposed a test
path assumption. The unchanged suite passes from the short isolated checkout
using its normal relative fixture root; neither issue was hidden or fixed by
changing producer/runtime semantics.
The build regression suite retains its Darwin/arm64 Metal-only test as SKIP
on Linux; that does not qualify or regress Metal. The ignored Rust signal test
is covered by the isolated interaction/lifetime path, not run concurrently
with ordinary unit tests.

## Non-claims and handoff

The native peer is synthetic: it does not establish Laya/another model's logits,
calibration, new model admission, inference performance or the actual Exon→DGX
chain. Existing independently qualified local model evidence retains its scope.
An approved installed listener, host pin, enrolled YAI key and admitted resident
finite engine were separate exits from that original fixture delivery; the
installed follow-up below now qualifies those producer facts. No remote SDK
method is claimed here; the concurrent YAI/SDK owner implements it.
The bounded input/result and conservative unknown-outcome/no-retry semantics
are specified in the public contract, not reconstructed from human CLI output.

`progression_decision=proceed`, `downstream_safe=true` only for typed SDK
consumer implementation against this producer. A03 remains READY.

## Installed LAN producer

`INTERFACES.FINITE.DECISION.LAN.OPERATIONAL.0` qualifies the actual Exon LAN
client, the explicitly approved dedicated SSH listener and a real finite CPU
engine in the operator Host on 2026-10-06. This is not a replay of a YAI Case.
The machine-readable [public operational handoff](data/finite-lan-20261006.json)
owns exact endpoint, approved server/client identities, source/build, model
lineage, generation, bounds and the two producer-authored control durations.
Operational addresses and process-local generations are observations, not
permanent product identities. Consumers must requalify generation after reload.

The installed source is `a46ddf5ee8c8b0fc4938a53f02fc372d9a00d116`, tree
`31387594c05c11e98a48a67c6fb92db19c60be7d`; executable SHA-256 is
`c7cfe0e0684bad13016154097ad14bfa08f30914b445c086276b912a048947ba`.
It was independently built, qualified and pushed from a clean checkout, then
installed through `make install` into an immutable commit prefix. The dirty
DeepSeek optimization candidate was neither discarded nor deployed.

Generic `tensor-program` registry admission authenticates the binding/source,
finite kind, non-generative strategy, row capacity and bounded CPU workspace
before engine publication. Unsupported CUDA and foreign source/binding profiles
fail closed. This adds no wire/public C layout and no family-name memory policy.

The operator explicitly approved a coordinated idle window. Preflight observed
zero active/HTTP/queued work, sessions, model leases and attached operator
clients before supported Host shutdown. The old Host completed teardown before
the replacement bound protocol 25. Its old service exit code was 3 after the
post-shutdown client observation failed; the recorded shutdown-complete event,
retired socket/listener and released singleton lock establish actual teardown,
not an inferred successful exit code. No active request was terminated.

The same DeepSeek model/artifact/binding/specialization and speculative strategy,
32768 context and 64-position prefill configuration were restored. Its
generation was 3; finite alias `laya-typed-finite-cpu` was generation 2 for the
original LAN controls below. An initial
idle reload used the registry's 4096 default; it was corrected through supported
unload/load with explicit 32768 before the restoration evidence. No user
session existed during that correction. Generation must not be used as a
durable substitute for model identity.

| Evidence plane / control | Authority and input | Expected | Observed | Claim earned |
| --- | --- | --- | --- | --- |
| Checkpoint/component reference | Existing exact `convaiinnovations/laya-typed-decisions` revision and independently captured upstream input/logits | Same 29-token three-candidate control; absolute tolerance `1e-4` | Two LAN responses preserve the previously qualified scores and exact six model/input-policy identities | Bounded numerical equivalence, not checkpoint-wide quality/calibration |
| Deployment/lifecycle | Real registered package, ordinary Host loader, admitted CPU resources | Finite-only kind/strategy, authenticated binding, bounded capacity and rollback | Real checkpoint loader, capacity refusal, cancellation/disconnect, stale generation, unload/reload and recovery pass | Exact CPU engine lifecycle, not CUDA or new model admission |
| Product path | Exon → approved restricted SSH → public C producer → operator Host | Matching approved peer/server, correlation, alias, generation and original candidate IDs | Positive and independent recovery complete; result identity agrees; one forward/backbone and zero sampling/generation | Installed public finite producer, not SDK/YAI semantic integration |
| Remote negatives | Stale generation and duplicate candidate IDs | No scores published for stale work; malformed population not dispatched | `YVEX_ERR_STATE` without result; duplicate population refused before dispatch | Fail-closed public producer |
| Cleanup/coexistence | Typed native Host/Engine snapshots after controls | Zero requests, queue, sessions, work, leases, attached clients and transient bytes; both engines remain ready | All zero; finite persistent workspace remains intentionally owned by its engine | No transient leak or retirement of DeepSeek |
| DeepSeek recovery | New synthetic non-thinking request, output bound 3, original model configuration | Terminal bounded response after coordinated replacement | HTTP 200, 8 prompt / 3 committed output tokens, terminal length bound | Producer remains executable; not a throughput/SLA or YAI Case qualification |
| Management preservation | Existing HTTPS process/identity plus native public `host.get` | Same approved TLS certificate, no management restart; current Host observable | Exon observes the same certificate digest; local public management sees running protocol-25 Host | Reachability and native management compatibility, not remote Studio acceptance |
| Source-stable software | Clean accepted source; mapped fast/structural, real Laya, restricted SSH, PTY and OpenAI lanes | No failing or blocked applicable checks | Fast 100 PASS; structural 18 PASS; real CPU, SSH, PTY and OpenAI 1 PASS each; docs reader builds | Changed-owner software/contract regressions; no new macOS/sanitizer or release claim |

Raw source-stable QA receipts, pre/post typed snapshots, real request/responses,
public pin observations and configuration backups remain outside Git at
`/home/dgmothx/lab/models/evidence/finite-lan-rollout-20261006.9Y2m9i`.
The four LAN controls include two full-model executions and two refusal paths;
they are not a workload/performance campaign. Durations are characterization of
this fixture only. The remote control's outer SSH round trip includes the DGX
control hop to Exon and is not pure client-observed Exon latency.

### Current generation after the coordinated idle window

The 2026-10-07 operator-approved idle replacement installs clean published
`a444bcdd384f6abfc79b07d1a26d93c17c4a97c0` through `make install` under its
immutable prefix. Runtime executable SHA-256 is
`e1ea8e02ad223a3fffb2ecc6839ee354658bd97a6e23c7e6b0651007d4ad3b4d`.
The machine handoff separates this **resident Host** from the unchanged public
forced-command producer at `a46ddf5e` and the independently running HTTPS
management listener. Neither transport was restarted or re-enrolled.
The previous Host had zero active/HTTP/queued work, sessions, leases, attached
clients and transient bytes. Its process, socket and singleton ownership retired
after one supported shutdown request before the replacement started; shutdown
ACK alone was not treated as completed teardown.

The finite engine remains **generation 1**, with specialization
`ff6f2d5c61e15657abbee3762fa2f0bdb71191f20437cce71afd7cc5d73b1228`.
All six durable model/input-policy identities, the approved server pins and
the YAI finite-only enrollment are unchanged. Consumers must use the current
generation, not the historical generation 2 in the original LAN receipt.

The current [machine handoff](data/finite-lan-20261006.json) separates those
historical controls from fresh installed public-C local recovery and four new
Exon calls through the approved public restricted-SSH listener. The
original three-candidate fixture again matches its independent checkpoint
reference within the predeclared tolerance; a request for retired generation 2
refuses with `YVEX_ERR_STATE` and no result. The result identity changes with
the generation by contract; equal model scores do not make result identities
interchangeable. Both fresh LAN positive/recovery calls preserve the six model
identities and ordered candidate IDs; stale generation publishes no result and
duplicate IDs refuse before dispatch. This renews the real producer/transport
control, not SDK/YAI semantic acceptance or low-latency Fast Search qualification.
No client private key was copied and no trust/grant was changed.

DeepSeek is restored at **generation 2** at this capture, with the same model,
artifact and binding, context 32768, chunk 64 and speculative
strategy. A new synthetic `Return OK.` recovery completes with HTTP 200 and
natural stop; no operator/Case prompt is replayed. Typed snapshots show zero
active/queued work, sessions, leases, attached clients and transient state.
HTTPS certificate inspection matches the previously approved pin; neither
public listener was restarted or re-enrolled. Further model reloads can change
DeepSeek's generation without changing the finite engine's generation.

Its specialization changes to the admitted computational candidate's
`3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144`;
this is not silently described as the prior deployment identity. Four fresh
coding controls from the actual installed Rust qualification command each
commit 256 tokens and preserve one token-ledger identity. The local receipt is
not upstream model-quality, target-only throughput, release or YAI evidence.
The [generated target detail](benchmarks/generated/qualification-deepseek-installed-native-coding-36.md)
projects that structured LOCAL receipt, retaining the first coding request
separately from the three subsequent samples and keeping unknown hardware/kernel
and resource-observation authority explicit. Its source/build context is bound
by the separately retained live-executable and source-capture witnesses.
The finite numerical control still matches the independently captured reference
within `1e-4`; malformed and overlong input publish no result. Persistent finite
workspace is intentionally owned, not leaked transient work.

Load observations in this window are **already-cached** single samples, not
cold-load improvements over the earlier residency observation. They do not
establish physical residency from a mapping or device address.

Raw post-window controls and snapshots remain at
`/home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL`:
`published-product-rollout-v33`, `published-product-controls-v34`,
`published-native-coding-v36`, `published-product-recovery-v37` and
`published-product-lan-v38`.
The failed outer-lock and exact-profile-as-model CLI attempts were refused
before dispatch and retained; the corrected ordinary `v4-flash --variant ...`
command neither redispatched indeterminate work nor replayed a Case prompt.
A direct non-SSH invocation of the forced-command adapter also refuses with
`restricted_ssh_required` before dispatch; it is not counted as successful
inference. Only the actual approved SSH invocation provides the renewed LAN
evidence. Its outer round trip includes the DGX control hop to Exon, not pure
Exon client latency.

### Exact consumer boundary and limits

Finite inference uses **restricted OpenSSH JSONL v1**, not the HTTPS management
router. The approved client retains only `finite-decision`, not remote
load/unload or Host control. A new HTTPS finite-computation route is not claimed.
The public contract owns input bounds, candidate/result identity, conservative
unknown-outcome semantics and no blind retry; the consumer never forwards or
encodes the private socket protocol and never parses human CLI output.

The CPU control remains expensive and is **not** low-latency Fast Search
qualification. Scores are uncalibrated and confer no Case authority. YAI/SDK
and Studio consumption, semantic search usefulness, broader input/quality,
performance targets, remote HTTPS computation and release readiness remain
independent gates. No private key, bearer or Case material is in this handoff.

`progression_decision=proceed`, `downstream_safe=true` for connecting the remote
typed YAI/SDK consumer to this exact installed producer only. A03 stays READY;
the separate DeepSeek competitive/residency delivery remains unfinished.
