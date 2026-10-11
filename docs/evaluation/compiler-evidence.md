<!-- docs:metadata
title: Compiler Evidence
id: yvex.evaluation.compiler-evidence
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Compiler Evidence

**How executable-composition evidence bounds a compilation claim.**

[Up](README.md)

## Executable composition oracle

The CPU-only tiny vertical is the fast composition oracle for this pipeline. Its focused test
owner deterministically generates an untracked GGUF, admits it through the production artifact
contract, compiles the semantic model, operator graph, Physical Execution IR and runtime binding,
then launches the real foreground host with zero engines, loads the fixture,
serves it, unloads it without stopping the host, reloads it as a new generation,
and admits two fitting engines concurrently. The production `host status`,
`engine list`, `engine load`, native generation, `engine unload`, `host logs`, and
`host stop` paths must return the expected context, text, identities, typed
completion event, routing refusals, and clean lifecycle. A second build must
reproduce artifact and binding identities, while a corrupted artifact must
refuse without terminating the host.

The fixture adds no production model family and does not establish support, quality, CUDA or
performance for a real model. Its generated artifact and binding remain temporary build evidence,
never repository authority.

## Program P: profile resolution and bounded allocation

The profile-driven continuation is **partial implementation**, not the competitive
exit of `V010.PHYSICAL.MODEL.COMPILER.GOAL.DRIVEN.0`. The canonical records below
bind the observed source delta, binary, requests, planned representations and
refusals. They contain no new inference throughput measurements.

| Subject | Earned evidence | Explicit boundary |
| --- | --- | --- |
| [DeepSeek 0731 allocation](benchmarks/generated/qualification-deepseek-0731-program-p-resolved-profiles-20261010.md) | Source-derived profile, bounded role frontier, exported policy, canonical plan and one internal source-preserving tensor probe | No new complete artifact, binding, model execution or independent quality claim |
| [Mamba2 profile/CLI](benchmarks/generated/qualification-mamba2-program-p-resolved-profiles-20261010.md) | Real catalog-guided terminal journey and replay; sequence-mixer geometry retained; unavailable allocation and Metal model execution refused | Prior Mamba execution proof remains separate; no new quantization or performance claim |
| Exact allocation algorithm | Exhaustive enumeration independently checks every frontier of 64 bounded test problems; ties, overflow, insufficient budget and state/output exhaustion are controlled | The optimized objective is source-retained elements under encoded bytes, not an estimated quality score |
| Receipt admission | Exact embedded-publication equality precedes automatic comparison; forged external numbers remain untrusted inspection | Missing kernel identity, changed workload/strategy and incompatible measurement bases refuse; no automatic qualified recommendation |
| Guided refusal/recovery | Real PTY cancellation, unsupported schema, source verification failure and request replay controls | A compiler target is not assumed to be a catalog alias; failed verification cannot publish an artifact or saved request |

The test owners are [optimization](../../tests/unit/optimization.c),
[Rust CLI contracts](../../tests/rust_cli_contracts.py),
[installed ABI](../../tests/test_public_abi.py) and the existing official
[tokenizer oracle](../../tests/reference/tokenizer.py). Source-stable raw controls,
source archives and software logs are referenced by the structured records.
Official encoding/BPE parity does not establish full-model logits or continuation.

### Reproduce the product path

```sh
./yvex compile optimize --list-techniques
./yvex compile optimize --guided --out-request goals.json
./yvex compile optimize --request goals.json --json
./yvex compile optimize --request goals.json --select <candidate-id> --out-policy recipe.json
```

The guided surface selects an acquired source through the native catalog. If its
canonical authenticated manifest is absent, it asks before invoking native source
verification; that operation may read all shards and retains its verified
manifest. Cancellation before verification changes no source state. A saved
request is reproducible intent, not proof of feasibility or a production receipt.

Request v2 may select `source-retention` with an explicit `weight_budget` and
bounded `search_states`. The exported policy feeds the existing `compile quant
plan`, `probe` and `emit --binding-directory` owners. See the
[compiler contract and examples](../architecture/compiler-ir.md#reproducible-engineering-workflow).
No step installs an engine. The ordinary guided surface currently stops at
candidate assessment; integrated guided production/qualification, pre-production
full workspace/state costing and evidence-backed recommendation remain open.

### Computational non-claim

The operator's previous DSpark remained resident during these offline controls.
The new role-allocation recipe was **not** run as a complete model. Existing
[DeepSeek performance records](benchmarks/generated/qualification-families.md)
remain historical, separately identified observations. In particular, the retained
MXFP4 publication records lack a kernel-bundle identity: the new comparison gate
does not fill that gap from the current build or rank those rows automatically.
Independent held-out quality and the 20/700 performance gates remain unearned.

## Mainline public planning and source handoff

The continuation based on `2de077e9` closes a bounded producer/consumer path,
not Program P's competitive exit. The implementation consumes the existing
compiler and source owners; it does not add another planner or qualification
store. The [public handoff](../contracts/product-management.md#physical-compilation-consumer-handoff)
owns request/result schemas and authorization boundaries.

| Control | Observation / oracle | Claim boundary |
| --- | --- | --- |
| Acquired-source preparation | Exact catalog path resolves the source-owned manifest destination; changed revision/provider and duplicate acquisitions refuse | No copied manifest or directory name is accepted as authentication |
| Real Mamba planning | Authenticated `mamba-codestral-7b-v0.1`, revision `4f086c08c1e0f07bdc50ca25125dbbf7475d21da`; CPU, context 32, prefill 4, concurrency 1, memory objective | Seven native candidates; 64 sequence-mixer layers, zero attention; no Engine or payload production |
| Independent Rust SDK | Disposable public same-user companion; persisted invocation, one dispatch, `job.get`, then exact CLI context/candidate comparison | Job recovery and native planning equivalence, not SSH enrollment, remote GPU execution or model quality |
| Published evidence association | Bounded metadata references by policy/transformation, followed by public `qualification.get`; same canonical records as CLI inspection | Related evidence is visible; unknown calibration/artifact correspondence cannot earn a recommendation |
| 0731 capacity | Existing authenticated MXFP4/Q2 artifact/binding; target-only, widths 512/1, independent 4K/16K/32K assessments | All observed peak envelopes refused before residency; available memory is volatile, not reserved |

Reproduce the independent consumer control against an isolated candidate binary:

```sh
cargo build --manifest-path sdk/rust/Cargo.toml --locked --example optimization
make YVEX_BIN=build/program-p-mainline/yvex RUST_PROFILE=dev client
```

`tests/integration/product_management_models.py --compiler-model MODEL
--revision REVISION --models-root ROOT --sdk-example EXAMPLE --evidence-dir DIR`
is the registered integration owner's optional real-source control. Its test
adapter receives the candidate executable via `YVEX_BIN`; ordinary product use
requires no environment setup. The disposable companion has its own config,
journal and public socket, and is the only process it retires. It never loads a
model or stops the operator Host. The source must already be authenticated.

The consumer test retains the exact CLI and SDK JSON, including native context,
candidate, source/tree/delta/build and Job identities. Capacity observations
remain separate from timed inference. No new load, TTFT, decode, prefill or
held-out quality result is inferred from these controls. Historical model
execution receipts remain the authority for their original configurations.

### Qualification fixture corrections

The `22329d17` hosted hermetic lane passed 135 controls. Its native macOS lane
reported 20 PASS, 2 FAIL and 4 BLOCKED; the local changed-owner matrix reported
133 PASS, 2 FAIL and 7 BLOCKED. These original receipts are retained, not relabeled.
The ensuing corrections concern qualification infrastructure:

| Defect | Correction / retained obligation |
| --- | --- |
| Finite peer relinked with an old public struct layout | Include its generated header dependencies in canonical runner membership; an incremental build regression changes a transitive header. |
| SDK test storage traversed Darwin's `/var` symlink | Canonicalize the fixture's temporary parent; production still refuses symlink profile roots, with a negative control. |
| Synthetic catalog peer closed before the consumer drained its replies | Retain the connection until the consumer closes it, matching Host lifetime; no protocol or timeout policy change. |
| Parallel sink failure selected a different terminal's poisoned-sink diagnosis | Test exact write diagnosis with one worker and abort/non-publication with four; numerical behavior and failure-selection policy are unchanged. |
| SSH SDK fixtures placed trust under world-writable `/tmp` | Use owned private cache children, retain `StrictModes`, and qualify real isolated SSH/native Host and tiny-model sessions. |

The independent connection-enrollment examples additionally require the
`native-credentials` feature and a usable native credential vault. A default
example binary is not that prerequisite. Configured credential-vault lanes
remain separate from typed SDK planning, local companion and SSH product tests.

The complete journey still has open gates: pre-production full workspace/state
costing, independently qualified candidate selection, an accepted dominant
execution correction, and comparable complete-model DeepSeek measurements.
The operator-owned old-checkpoint generation is not a disposable 0731 fixture.

The inherited `2de077e9` hosted `integration.repl` failure was reproduced locally:
the fixture expected automatic session reuse after a different Host incarnation.
The current client correctly fences that session. The corrected regression
requires refusal, no prompt dispatch, unsuccessful stale detach and explicit
new-client recovery; it changes no runtime or terminal implementation.
