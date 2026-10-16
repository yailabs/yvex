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
| 0731 capacity before the exclusive window | Existing authenticated MXFP4/Q2 artifact/binding; target-only, widths 512/1, independent 4K/16K/32K assessments | All observed peak envelopes refused before residency while the operator engine remained resident; available memory is volatile, not reserved |

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
remain separate from timed inference. No load, TTFT, decode, prefill or held-out
quality result is inferred from these planning controls. The subsequent
authorized execution has separate [receipts and scope](#mainline-native-0731-window).

### Qualification fixture corrections

The `22329d17` hosted hermetic lane passed 135 controls. Its native macOS lane
reported 20 PASS, 2 FAIL and 4 BLOCKED; the local changed-owner matrix reported
133 PASS, 2 FAIL and 7 BLOCKED. These original receipts are retained, not relabeled.
The ensuing corrections concern qualification infrastructure:

| Defect | Correction / retained obligation |
| --- | --- |
| Finite peer relinked with an old public struct layout | Include its generated header dependencies in canonical runner membership; an incremental build regression changes a transitive header. |
| SDK test storage traversed Darwin's `/var` symlink | Canonicalize the fixture's temporary parent; production still refuses symlink profile roots, with a negative control. |
| Synthetic catalog peer closed before the consumer drained its replies | Retain catalog connections like the Host; finite trace subscriptions instead close after ACK and exercise queued-frame draining. |
| Parallel sink failure selected a different terminal's poisoned-sink diagnosis | Test exact write diagnosis with one worker and abort/non-publication with four; numerical behavior and failure-selection policy are unchanged. |
| SSH SDK fixtures placed trust under world-writable `/tmp` | Use owned private cache children, retain `StrictModes`, and qualify real isolated SSH/native Host and tiny-model sessions. |

The independent connection-enrollment examples additionally require the
`native-credentials` feature and a usable native credential vault. A default
example binary is not that prerequisite. Configured credential-vault lanes
remain separate from typed SDK planning, local companion and SSH product tests.

The `b03631be` hosted hermetic lane passed 135 controls; native macOS retained
23 PASS, one real tiny-vertical FAIL and two unconfigured SDK-vault BLOCKED
controls. The earlier trace-fixture retention did not fix the real transport:
Darwin can reject timeout updates after shutdown while complete frames remain
queued. The ensuing transport correction requires observed peer hangup before
accepting that socket-option condition; invalid bounds, live errors and EOF
still refuse. A real Host retirement regression drains its terminal ACK only
after the peer has closed. The trace fixture again follows the production
close-after-ACK lifecycle. Hosted native enrollment builds the actual
`native-credentials` examples; a developer session without a usable vault
cannot claim that lane from passing typed SDK tests.

The complete journey still has open gates: pre-production full workspace/state
costing, independently qualified candidate selection and an accepted dominant
execution correction.
The operator-owned old-checkpoint generation is not a disposable 0731 fixture.

The inherited `2de077e9` hosted `integration.repl` failure was reproduced locally:
the fixture expected automatic session reuse after a different Host incarnation.
The current client correctly fences that session. The corrected regression
requires refusal, no prompt dispatch, unsuccessful stale detach and explicit
new-client recovery; it changes no runtime or terminal implementation.

## Mainline native 0731 window

The operator explicitly authorized temporary unloading of only DeepSeek after
software QA. Source `3c73c2a5`, tree `7fcc485fbaba0f5fb7d72e472d8e7ed2fc8a354d`,
and the captured executable were stable throughout the native samples and the
separate CUPTI diagnostics. The existing authenticated MXFP4/Q2 artifact
`25d29155…` and binding `091f933e…` were reused: no model recompilation, cache
eviction, reduced reserve, Host replacement or model-sized duplicate allocation.

| Lane | Exact control | Result authority |
| --- | --- | --- |
| Coding target-only | Context 4096, chunk 512, greedy/none, 31 new input positions, 256 committed output tokens, three fresh sessions after separate warmup | [Native target-only receipt](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-target-only-coding-hash-table-20261011.md) |
| Uncached 2K | Context 16384, chunk 512, exactly 2048 new positions, 16 output tokens, three fresh sessions | [Native 2K receipt](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-target-only-prefill-promessi-2048-20261011.md) |
| Uncached 8K | Same context/chunk, exactly 8192 new positions, 16 output tokens, three fresh sessions | [Native 8K receipt](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-target-only-prefill-promessi-8192-20261011.md) |
| Coding DSpark | Same coding case, context, sampling and output population; three fresh sessions | [Native speculative receipt](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-speculative-coding-hash-table-20261011.md) |
| Device attribution | Separate instrumented complete requests: coding with 64 output tokens, 2K/8K with 16 | [Coding](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-critical-path-20261011.md), [2K](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-critical-path-prefill-promessi-2048-20261011.md), [8K](benchmarks/generated/qualification-deepseek-0731-program-p-mainline-critical-path-prefill-promessi-8192-20261011.md) |

The generated receipts own all benchmark values, sample distributions, server
and client TTFT, arrival gaps, complete-request time, load observations and
speculative economics. Client content arrival is not terminal painting.
Coding reports committed tokens after the first divided by post-first decode
wall time; proposals and discarded tokens are not counted as output.

Both 4K and 16K admitted and executed after the authorized unload. This does not
invalidate the earlier resource refusals: their available-memory observations
differ. It does not qualify 32K for this artifact. Post-load smaps and runtime
placement are retained separately; RSS/PSS/file pages and CUDA addressability
are overlapping descriptions, not additive model copies. No whole-model physical
residency or pinning claim follows from a pointer or `Locked=0`.

All ordered fragment channels, byte lengths and SHA-256 values matched the
retained same-artifact native controls. This is an internal regression control,
not an independent 0731 reference or representation-quality certificate. The
single chat-short warmup showed a speculative slowdown, separately from the
repeated coding advantage. High/maximum, HTTP, concurrent and installed-product
performance are not qualified by this window.

The profiles identify substantial encoded-matrix/expert execution in coding;
long prefill additionally spends material device time on attention reduction
and candidate scores. Kernel population durations, host scopes and transfer
counters have different meanings: attention/MoE host scopes overlap, logical
bytes are not physical DRAM traffic, and no bandwidth or hardware ceiling has
been established. No new numerical class, prepared layout or kernel is accepted
from those observations alone. Mainline throughput is consistent with the
retained baseline, not a competitive acceleration; 20/700 remains unearned.

Raw authority is
`/home/dgmothx/lab/models/evidence/program-p-mainline-20261011.WAxMHFPg`:
`source/` retains source/build/executable bytes, `attempt-3/` native runs, and
`diagnostic/` CUPTI runs. Earlier adapter refusals remain alongside the successful
window: unsupported unload syntax did not mutate the service; a subsequent
observer initially confused an unloaded catalog record with a resident engine.
Continuation used observed successful unload state, not an uncertain retry.
The same operator profile/artifact/binding/context 32768 was restored after
each isolated lane group, ending at generation 3. Installed Host `337e7e73`,
listeners and grants were unchanged. Generation identity was not reused.

## Indexed MoE tile realization

Program P's next executable candidate addresses the routed matrix population
identified in the retained complete-request profiles. Previously every output
tile rediscovered its expert through a prefix scan. The existing device worklist
producer now emits a compact tile index shared by gate/up and down, and the
matrix consumers traverse column tiles before output rows. No encoded weight,
source operation, expert selection, reduction tree or publication rule changes.

| Owner | Implemented decision / boundary |
| --- | --- |
| Semantic graph and deployment | Existing sealed routed operation, operand compatibility, admitted widths and `DEVICE_MATRIX_TILE` class; the minimum population remains four. No family-name selector or new numerical class. |
| CUDA worklist | Derived `(bucket ordinal, column offset)` descriptors over actual eligible populations, including sparse expert IDs and tails; produced on the same execution stream. |
| Matrix consumers | Direct descriptor lookup and column-major task order, with the previous null-index scan retained as an exact control. Conservative padding creates no new rows. |
| Capacity / lifetime | Existing backend workspace calculation and arena use the same checked bound. The index is phase-local, not a prepared weight copy or persistent model allocation; existing runtime maximum/overlap accounting still applies. |
| Program P / product | Equivalent implementation under an already admitted class. Source/build/kernel identity changes; source, parameter artifact, binding, public schema and CLI/SDK grammar do not. This is not an independently qualified automatic recommendation. |

### Component comparison and correctness

[Generated component comparison](benchmarks/generated/cuda-moe-indexed-tiles-20261011.md)
owns the measurements, exact geometry, sample count, dispersion, binary/kernel
hashes and frozen source delta. It uses CUDA events around the worklist builder
plus one projection, with alternating control/candidate order and a separate
warmup. Both arms use the same binary. Warm synthetic allocations are not the
complete model's mapped weights or routing distribution; cache locality is the
mechanism being tested, not a measured cache-hit or bandwidth claim.

| Evidence plane / control | Authority and result | Claim earned |
| --- | --- | --- |
| Backend numerics | `cuda.moe_rows`: independent decoded-weight scalar F64 reference under the existing tolerance; exact comparison against prior scan and Q2 row execution | Bounded IQ2/Q2 arithmetic, BF16/exceptional recovery and output-bit preservation; not checkpoint logits or model quality |
| Derived populations | Real device builder versus independently enumerated host worklist, sparse expert IDs, mixed scalar/matrix populations, poisoned storage, malformed descriptors and canaries | Correct pair association, bounds and fail-closed consumption, not fabricated batching |
| Runtime composition | `cuda.info`: complete bounded encoded MoE, graph capture/replay, workspace reuse, cancellation, incompatible-kernel refusal and caller-owned results | Changed path composes with existing resource/lifecycle owners; not DeepSeek inference |
| Device safety | Compute Sanitizer memcheck: zero errors; racecheck: zero hazards/errors/warnings | Device accesses and shared-memory synchronization in the bounded fixture |
| Component performance | Repeated warm control/candidate samples in the generated observation | Characterized builder/projection benefit only |
| Family / checkpoint / representation quality | No new independent checkpoint oracle, NLL/PPL, KL or held-out task evidence | UNQUALIFIED / BLOCKED; no inherited quality promotion |
| Complete-model deployment / native product | New 0731 comparison not executed; the prior exclusive window has expired | UNQUALIFIED; no new decode, prefill, TTFT, request-latency or model-memory claim |

The frozen component source is main `683ee840` plus delta
`91d44be3aae8ed7ded52e9a8ffbc103f6699f13bf93167082db25a3cd7d509a8`.
Focused QA receipts are `aedc91a4…` (`cuda.moe_rows`) and `e43c2d5d…`
(`cuda.info`) under `build/qa/evidence/`; device safety logs are
`build/program-p-mainline/tile-column-{memcheck,racecheck}.log`.
Raw experiments, exact harness, patch and executable hashes are retained under
`/home/dgmothx/lab/models/evidence/program-p-tile-dispatch-20261011.xw7JkaVr`.
These observations precede documentation publication and retain their original
source delta rather than claiming a later clean source identity.

### Rejected or deferred alternatives

| Alternative | Experimental reason / scope |
| --- | --- |
| Compact index without column-major scheduling | Only marginal component improvement; retained as a causal control, not the selected mechanism alone. |
| Fixed-Q2/block-count template specialization | Gate/up component regressed and generated register usage increased; rejected. |
| Packed integer minimum products | Down improved but gate/up regressed in the tested generic variant; not integrated. |
| Shortened zero-leaf reduction | Modest component gain with additional arithmetic proof obligations; not integrated into this candidate. |
| Lower matrix eligibility to one | Single-row gate/up and down regressed against scalar execution; retain deployment's existing threshold. No invented width or replicated inputs. |

The associated raw `component-*` files retain these exploratory observations;
they are not final full-model selections. No archived prepared-IQ2 experiment
was reactivated. Warming and managed-prefetch negatives remain unchanged.

### Remaining acceptance

The operator Host `337e7e73` and original DeepSeek generation 3 were observed
idle and preserved during the bounded tests. No unload, model-sized duplicate,
cache eviction or installed executable replacement occurred. Sampled idle
observations and the advisory device lock are not an exclusive reservation.

A fresh authorized window is required to compare the existing authenticated
0731 artifact/binding at 4K coding and 16K-context uncached 2K/8K prefill,
target-only and DSpark separately, then restore the original operator profile.
The new candidate's 32K admission is not established. Small-population decode,
wide projections and long-history attention retain material costs outside this
matrix scheduling correction. Their profile durations are not additive wall
shares or a hardware ceiling. Program P and the 20/700 exit remain open;
component improvement is not the requested complete-model result.

### Source-weight IQ2 producer comparison (2026-10-15)

This bounded checkpoint belongs to the selected Program P Task, not a new
model-quality or competitive-performance qualification. Its complete retained
campaign root is
`/home/dgmothx/lab/models/evidence/program-p-convergence-20261012.VxnrFZDv/campaign-output-recovery-20261015.8QuK4Xg7`.

The next discriminator uses authenticated official 0731 source operands,
real population-normalized bootstrap statistics and the independently pinned
ggml encoder/decoder. Source/role/expert association comes from sealed
Transformation IR and compiled MoE operands, not tensor-name heuristics.
The examined source-derived Q2 blocks reproduce the retained artifact exactly.
Every IQ2 block is checked against the independent decoder; format compatibility
is distinct from reconstruction fidelity.

The native v1 IQ2 producer loses materially more calibration-weighted fidelity
than the independent producer on these operands. Its one unweighted grid lookup
and proxy magnitude fit differ from a weighted codebook search over the actual
decoded levels. The new explicitly selected v2 producer improves the observed
reconstruction objective on gate/up operands from hash-routed and learned-routed
layers, at unchanged IQ2 bytes. Q2 remains more faithful in the examined controls.
The full native implementation reproduces all retained prototype parameter
payloads exactly. This is not a model-quality or throughput qualification.

The retained campaign directory contains the exact sources, binaries and raw
receipts: `source-expert-codec-independent-20261015-01`,
`source-expert-iq2-population-20261015-01`,
`source-expert-native-iq2-v2-20261015-01` and
`source-expert-native-iq2-population-20261015-01`. They preserve per-role
reconstruction errors, encoded bytes, construction/verification time and exact
decoder populations. No aggregate full-model speedup is derived from them.

Policy v3, versioned quant execution and writer provenance now carry the
explicit producer choice; v1/v2 policies and default encoding remain controls.
Only observed, complete selected operands may supply the exported calibration.
Its normalized DAT divisor is one by construction, not a claim of one real
observation; original per-population counts and capture identities remain in
the retained receipt. The generic association now accepts a strict bound
manifest through the existing `compile quant plan --imatrix-manifest` path.
Its source is checked against sealed transformation facts; numeric bytes,
context and producer become calibration identity rather than family constants.
Legacy raw data retains its prior explicitly declared provenance and identity.

`calibration-source-bound-profile-20261015-01` separates semantic dataset/context
projections from forensic receipt paths and timestamps. The corresponding
`observed-iq2-bound-native-plan-20261015-01/receipt.json` retains the frozen
source delta, actual CLI executable/build, policy, data and writer-plan identity.
It selects 56 fully observed gate/up terminals; the remaining 15 main layers,
down projections and support parameters retain their previous encodings.
Encoded bytes and compatibility reported by that plan are predictions, not
resource admission, artifact integrity or model performance.

Experimental production uses the canonical writer, independent reader and
binding path. Full artifact completion, resource admission, actual-model
measurements and independent held-out quality remain open. No service or
Engine change follows from the CPU codec/production checks.

The calibration lifetime control additionally demonstrates a source defect:
the prior executor could accept a changed borrowed calibration file after
execution had begun. Native terminal commit and final completion now revalidate
the supplied owner. A deterministic append during sink delivery refuses before
the first terminal commit; stable ownership and a fresh post-refusal owner
complete normally. `calibration-lifetime-before-after-20261015-01/receipt.json`
retains the same fixture against the frozen prior and repaired native libraries,
with failure/success respectively. This is a synthetic resource-lifetime
control, not evidence that real model calibration changed or that quality passed.

The source-bound entrypoint also checks the explicitly declared legacy raw-DAT
source against its independently required checkpoint identity. The synthetic
`calibration_source_refusal_witness.c` accepts a foreign required source before
that correction; matching legacy identities remain unchanged. This refusal is
not authentication of an otherwise unverified collection history.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Native IQ2 reproduction | Pinned independent ggml decoder and retained prototype | Source-derived routed gate/up population | Exact decoder and native-producer reproduction | Retained component receipt agrees | Exact bytes; weighted reconstruction loss reported separately | PASS | Producer implementation, not model quality |
| Calibration admission | Sealed source identity and exact numeric-file digest | Matching, foreign, malformed and drifting profiles | Preserve identity or refuse without terminal commit | Four affected native unit targets pass; before/after witnesses discriminate the defects | Exact identities and refusal/cleanup assertions | PASS | Source association and borrowed-data lifetime |
| Quant sanitizers | Registered ASan/LSan and UBSan lane | Complete quantization fixtures | No sanitizer error | Full lane passes on the retained code snapshot | Zero reported sanitizer errors | PASS | Quantization software safety within fixture scope |
| Structural recovery | Registered structural owners | Canonical ownership, ABI, build and documentation | Complete required checks | 18 PASS and two resource-throttled timeouts; both timed-out targets pass after bounded build-resource adjustment | Exact test identities; original ERROR records retained | PASS after rerun | Structural checks, not inference performance |

The final native unit controls and structural reruns retain source delta
`1b5b4913…`; the preceding full sanitizer lane retains `b363911e…`.
Between those snapshots, only fixture tracking and a policy-v3 diagnostic
changed. Exact run identities remain in the canonical QA receipts.

### Reusing package admission for executable compilation (2026-10-16)

`bench attention prepare --admitted-binding FILE` composes existing binding
authentication, source compilation, artifact hashing and physical compatibility
owners. Runtime reopens the compatible binding; compilation consumes its copied
package admission, reattaches the current file snapshot and independently hashes
the artifact. It rebuilds numerical semantics and executable plans instead of
borrowing them from the old binding. Production proof, prior admission and legacy
representation rebinding remain mutually exclusive. No source weights are encoded
again and no Engine is loaded or changed.

| Evidence | Authority / fixture | Observed | Claim boundary |
| --- | --- | --- | --- |
| Typed bridge | Native unit fixture with authenticated binding and observing compiler callback | Requested preparation choice survives; foreign adapter/transform and ambiguous admission refuse before compiler entry | Ownership, copied admission and refusal, not actual model compilation |
| Real package | Previously produced Mamba artifact `6038bca5…`, binding `bee48031…`, exact source and physical plan | Catalog-only control refuses; readmission succeeds; replay returns the identical new `544d7bf6…` binding | Offline real pure-SSM compilation, not inference or quality |
| Filesystem lifetime | Source-stable first negative control followed by current-snapshot repair | Full file SHA remains unchanged; persisted package facts do not substitute for the current inode/timestamp snapshot | Fresh file authentication before compatibility; no byte-integrity bypass |
| CLI and cleanup | Registered CLI and runtime-binding controls | Help, wrong-action refusal, no partial published result and deterministic replay exercised | No public management action, SDK schema change or installed rollout |

Raw exact identities, source/build capture, commands and receipts are retained
under the campaign root above in `admitted-mamba-cli-20261016-01` and `-02`.
The real-package control uses frozen Program P source delta `001c14e6…` and
binding v19; current-main interface compatibility is a separate software gate.
The former retains the repaired snapshot-admission failure; the latter is the
successful, source-stable control. Package production and executable numerical
qualification remain independent: this mechanism transfers neither quality nor
performance evidence to a different realization.
