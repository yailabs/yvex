<!-- docs:metadata
title: Retained Execution Observations
id: yvex.evaluation.retained-observations
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Retained Execution Observations

**Preserved measurements and their limits, not a new qualification run.**

[Up](README.md)

These observations were retained at the pre-migration source
[`b5f632ef`](https://github.com/yailabs/yvex/tree/b5f632ef8d452f61bbbc8f91f9459a2fd26b7c83).
They are not rerun by documentation validation. Scope and missing provenance
remain visible; [Status](../project-control/STATUS.md) alone owns current maturity.

## Provider progress

The transport correction at `6ae29730` qualified actual progress forwarding,
bounded inactivity, connection saturation, discovery, disconnect cancellation,
telemetry overflow and tiny-model provider execution. It did not rerun or qualify
the retained long request below. A transport fix cannot retroactively turn an
earlier HTTP 504 into a successful model result.

## Mamba finite readout

The exact live control used prefix `[1]` and candidates `[3]`, `[4]`, `[3,4]`
and `[3]`. One prefix forward fed five teacher-forced candidate steps and two
logits rows with one backbone, zero sampler calls and zero generated tokens.
The multi-token score was `-31.875254551685494` versus independent long-double
reference `-31.8752545516854968536` (`max_abs=3.144468487706128e-15`, tolerance
`1e-12`); order and full-prefix replay differences were zero. Shared committed
state identity was unchanged across success and cancellation. The [structured duration observation](benchmarks/generated/mamba-readout-characterization.md) retains the bounded CPU timings and their unavailable context. This is characterization, not a performance claim.

## Qwen profile and hybrid readout

The exact ordinary Qwen lane observed profile identity
`8933f7f1d36d1fafdcd71034d517c0c33291d9c08d082494d6e3b0c5c0cd87d5`,
CUDA bundle `3028627c3fd9220cd498998200fc61f336d4893797776bc5b5e0a104fd5215ae`,
DEVICE_NATIVE class, COMPATIBLE_DEGRADED attention and MoE, and EXACT sampling
for the explicitly not-invoked readout workload. Its existing direct-versus-
attached 248,320-logit control remained bitwise exact and printed
`decision_readout=not-invoked`. Resealed false-EXACT attention/MoE profiles,
stale generation/specialization, foreign bundle/class and malformed records
were refused. The exact Mamba CPU V0 replay retained its independent score,
order, replay, cancellation, zero-sampling and zero-generated-token evidence.
This closes profile truth only, not Qwen readout breadth.

`DECISION.READOUT.QWEN.BREADTH.0` is COMPLETE at the exact admitted artifact
and binding in the [Qwen dossier](../model-families/qwen3.8-text.md). One shared hybrid prefix `[1]` feeds candidates `[3]`,
`[4]`, `[3,4]` and the opaque-ID alias `[3]`. The multi-token likelihood is
`-24.020690479888565`; independent long-double log-sum-exp over admitted logits
gives `-24.0206904798885572547` (`max_abs=7.284466773623598e-15`,
tolerance `1e-12`). Independent full-prefix-per-candidate replay has maximum
error `1.0887179364996641e-14` against that arithmetic oracle; order difference
is zero. Cancellation publishes no partial result; retry preserves scores.
Sampling/generated counts are zero, the resident backbone count is one, and
the common hybrid source-session identity is unchanged.

The [structured allocation observation](benchmarks/generated/qwen-readout-state.md) retains the shared/branch/workspace observations. Mapped model bytes remain 53,815,809,152; addressability is not a second resident copy. These are bounded allocation observations, not continuous device/process peaks. The same live harness retains the Mamba CPU oracle and lifecycle
control. No public API, calibration, semantic authority, universal model
support or upstream whole-model conformance follows from these controls.

## DeepSeek numerical classes

The DeepSeek numerical gate exposed three independently necessary CUDA
differences in the decoded-forensic class: decoded F32 row matvec accumulated
in parallel F32 rather than source-order F64; weighted-attention/mHC square
sums used F32 rather than F64; and forensic attention used online rather than
the CPU two-pass maximum/accumulation order. The common CUDA owners now express
the declared forensic class without a DeepSeek family branch. In the admitted
two-token full-evidence control, all 43 layers, final hidden values and all
logits agree with CPU exactly (`max_abs=0`, `rmse=0`, argmax `339/339`,
finite-population total variation `0`). Same-backend chunk/whole state
identities agree; CPU-versus-CUDA persistent attention-state digests are
layout-bound and deliberately not compared. Removing any one of the three
corrections left a nonzero hidden error (`0.65625`, `0.28125`, `0.0625`,
respectively), so their interaction is an observed numerical cause, not a
post-hoc tolerance increase. The native attention reduction and Q8 activation
remain separate execution classes. Production CUDA completed two bounded
128-token whole/chunk controls with identical within-CUDA state/hidden digests
and finite output, and the live target-only/DSpark generation lane completed;
these controls do not prove CPU equivalence for Q8 or absence of all future
non-finite cases. The earlier failing observations in the next section remain historical
pre-correction evidence, not current qualification.

## Compiler cutover limits

Earlier cutover observation, before the decoded-forensic repair above; execution classes must not be conflated:

DeepSeek target-only and DSpark generation, schedule consumption, state
preservation and output-head component oracles pass on CPU/CUDA. Given the same
admitted hidden input, the CUDA output-head reference comparison observed
`max_abs=7.62939453125e-06`; the retained broader CPU/CUDA comparison still
observed `max_abs=9.982114791870117`, consistent with the pre-existing
approximately 9.982168 gap. It is therefore retained as OPEN independent
full-model conformance evidence, not normalized away or classified as a
compiler-cutover regression. Official DeepSeek vectors and authoritative
upstream Qwen conformance were not available for reproducible execution.

MiniMax audio CPU/CUDA programs reproduce their exact retained fixtures
bitwise; text-layer CUDA observes `max_abs=0.03125` within its registered
tolerance; joint-program replay preserves 128 execution values and 768 profile
values exactly while malformed capacity, target, aliasing, transaction and
cancellation cases fail closed. The exact complete 50-block fixture set is not
configured, so full-scale/whole-model evidence remains
`BLOCKED_BY_ASSET_IDENTITY`; bounded component preservation is not promoted.
The Mamba limit in that earlier cutover observation was superseded by the [exact artifact execution record](../model-families/mamba2.md); it is not the current family limit.

The source-stable mapped campaign, including compiler/program negatives,
binary/import refusal, transaction, rollback, cancellation, stale publication,
CPU/CUDA and ASan/LSan/UBSan lanes, qualifies the current cutover at exactly
that scope. A no-NVCC lane initially encountered a stale external dependency
prefix rather than a product failure; the lane now owns an empty verified
prefix and passes two consecutive builds. Missing upstream references, live
assets and performance baselines remain BLOCKED/NOT RUN, never PASS.

## External long-request characterization

Real external prefill execution is retained as `.1` characterization
(R / S / Q), not a reopened capacity defect or a compiler-cutover closure gate.
The selected closure criterion remains the implemented and qualified universal
consumer/lowering cutover. Golden latency does not block that boundary, and
removing it as a gate does not turn its failure into a pass or qualify YAI's
external lifecycle. The unchanged 40,277-byte Golden
request tokenizes to 12,055 inputs and fits the explicitly configured
16,384-token deployment with 4,329 output tokens available. Exact CUDA candidate
scoring and cooperative ranking preserve exact numerical results and ordered head reduction,
candidate identities/ties/refusals and capacity-stable graph replay, qualified
by the [independent selection oracle](../../tests/unit/cuda/attention_selection.c),
device memory/synchronization checks and real target/DSpark regressions.
Native reduction retains lane-local accumulators, encoded expert rows specialize
admitted geometry, and ordinary dots defer operand-finiteness rescanning to
exceptional results. The [reduction](../../tests/unit/cuda/attention_reduction.c),
[expert-row](../../tests/unit/cuda/moe_rows.c) and
[finite/exceptional-dot](../../tests/unit/cuda/dot_finiteness.c) oracles preserve
numerics, finite-overflow recovery and fail-closed invalid operands. These are
backend mechanism repairs, not new physical recipes or compiler cutover claims.
Encoded projections improve weight-row locality and expert dots reuse the
canonical IQ2 lookup table in block-local storage without changing dot order.
A separately demonstrated block-softmax shared-reduction race is repaired at
its storage-reuse boundary; the [bounded softmax oracle](../../tests/unit/cuda/attention_softmax.c)
qualifies causal/finite/negative behavior and exact repetition, with zero
reported device memory, synchronization and race errors at that scope.

The retained pre-transport-correction unmodified-request replay uses loaded weights but a fresh
session and zero reused prompt tokens: 918 tokens in 42.73 s, 5,532 at 276.01 s
and 10,884 at 567.85 s. Its last progress is 11,694/12,055 in 613.70 s
(19.05 tokens/s cumulative); producer HTTP 504 arrives at 616.18 s, with zero
generated tokens and clean cancellation/session retirement. The subsequent
five-input/one-output-token control still returns HTTP 200. Complete prefill,
first-token latency and normal response completion remain unqualified; the
local-protocol timeout is unchanged. Three bounded GPU traces of the current
published binary separate scoring from ranking: near 600, 1,806 and 3,600
processed tokens, mean scoring takes 37.12, 54.29 and 79.00 microseconds;
ranking takes 19.84, 25.94 and 44.18 microseconds. MoE/projection and attention reduction dominate
the sampled device work; stream synchronization includes device waiting, not
an independently additive bottleneck. The late-window MoE-up launch classes
must remain distinct: 9,216 blocks average 1,225.20 microseconds, while 1,536
blocks average 276.24 microseconds. Isolated lookup improvement does not prove
equivalent improvement across these real populations. Qualified repairs have not closed
the real request. Further performance work must preserve numerical and
candidate semantics and qualify complete-request behavior across the prompt,
not just its first prefix or isolated kernel.

For that retained full-request boundary, `downstream_safe=false`: bounded component or transport evidence does not close the external request.

The [GB10 workload/measurement authority][gb10] constrains replay. The retained
pre/post control uses clean source snapshots, 44 input tokens and 256 committed
output tokens per measured run, target-only greedy execution, one session and
the same exact deployment. Three sequential warm repeats characterize this
boundary; thermal/clock observations are not a randomized causal experiment.
No architecture-level speedup, upstream conformance or release gate is claimed.
[gb10]: benchmarks/gb10-targets.md

## Source acquisition control

Deterministic controls qualify terminal disconnect/reattach, structured retry,
slow progress, bounded stall, provider and supervisor loss, explicit stop and
exact-generation resume, PID reuse, stale/malformed state, lock refusal,
TTY/NO_COLOR/log/JSON projections and finalization. A real immutable HF source
(`hf-internal-testing/tiny-random-gpt2@71034c5d8bde858ff824298bdedc65515b97d2b9`)
completed 487,753 selected bytes across seven files including one Safetensors
object, then reopened through the normal verification receipt. The existing
roughly 510 GB V4.1 transfer remains motivating evidence, not a mutated fixture.
Acquisition completion still does not establish model support, artifact
readiness or release qualification.
