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
finite engine are still needed for actual remote integration. No remote SDK
method is claimed by this delivery; the concurrent YAI/SDK owner implements it.
The bounded input/result and conservative unknown-outcome/no-retry semantics
are specified in the public contract, not reconstructed from human CLI output.

`progression_decision=proceed`, `downstream_safe=true` only for typed SDK
consumer implementation against this producer. A03 remains READY.
