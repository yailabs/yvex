<!-- docs:metadata
title: Internal Numerical Execution ABI
id: yvex.contracts.numerical-abi
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Internal Numerical Execution ABI

**Exact prefill, MoE, transformer, logits and sampling contracts.**

[Up](README.md)

## Embedding row selection

The existing F32 embedding primitive reads a table described as
`[hidden_size, vocabulary_size]`, with contiguous vocabulary rows of
`hidden_size` F32 values, and writes `[token_count, hidden_size]` in token order.
Each token ID must be in the table vocabulary. The primitive selects source
representations without arithmetic, conversion, reduction or finite-value
reclassification; its numerical criterion is exact F32 bit equality, including
signed zero, subnormal and exceptional payloads. Repeated IDs repeat identical
rows. This is narrower than model-input admission, which may require finite
payloads under its own contract.

The Metal foundation realizes that existing operation with integer payload
loads/stores. Qualification compares original host bytes through independent
row addressing, separately from the existing CPU backend operation. It does
not infer equivalence for operations requiring ordered F64 accumulation or
other numerical classes. F16 conversion, matmul, normalization, attention and
quantized variants remain explicitly unsupported by this Metal realization.

## Ordered decoded-dot publication

The ordinary decoded CUDA projection publishes F32 from a source-ordered F64
dot over decoded F32 weights and F32 activations. Any subsequent additive or
BF16 publication remains a separate, unchanged operation. Q8-activation
reduction has its own admitted numerical class and is not this contract.

The competitive execution Task integrates a **certified equivalent realization** at
the CUDA owner; this is not a relaxed tolerance or an unordered-F32 class.
For each result, a warp computes a parallel F64 dot and a conservative interval
containing the literal source-ordered result. It may publish only if both
interval endpoints round to the same F32 bit pattern. Otherwise it evaluates
the original ordered dot on CUDA. Forensic projection retains literal ordered
evaluation. No CPU fallback, changed weights or changed routing is involved.

For finite F32 operands, each product is exact in F64. With `u = 2^-53`, width
`n`, and parallel depth `d = ceil(n / 32) + 5`, the serial and parallel forward
error bounds are respectively `gamma_n * A` and `gamma_d * A`, where
`gamma_k = k*u / (1-k*u)` and `A` is the sum of absolute products. For
`n <= 2^50`, `2*(n+d)*u*A` conservatively bounds their difference. The norm is
bounded upward using directed F32 multiplication/addition; it is **only a proof
bound**, not the dot accumulator. Norm overflow disables certification. The
radius is rounded upward in F64, and interval subtraction/addition downward
and upward respectively before round-to-nearest-even F32 conversion.

For narrow block-owned F32 matrices of width at most 32768, an inconclusive
global-norm/lattice proof may use a tighter prefix enclosure before literal
recalculation. Each 256-term chunk has a parallel F64 sum of depth at most 13
and an upward norm `N`. Let `T` approximate the real prefix with error `E`, and
let `F` bound the difference between the literal ordered prefix and the real
prefix. With `g_m = m * 2^-52 >= gamma_m` for the bounded chunk length:

```
F_next = (1 + g_m) * F + g_m * (abs(T) + E + N)
E_next = E + g_13 * N + g_1 * (abs(T) + abs(chunk_sum))
T_next = RN_F64(T + chunk_sum)
```

All bound arithmetic rounds upward. Final publication requires identical F32
bits at `T - (E + F)` and `T + (E + F)`, with outward rounding. This certifies
the original ordered computation, not just the exact mathematical sum.
An unproved endpoint, nonfinite bound or unsupported geometry retains literal
ordered CUDA evaluation. No epsilon is used to accept a different result.

Products of finite F32 values and these bounded sums cannot underflow or
overflow F64. Upward F32 norm underflow remains conservative. Distinct signed
zeros do not certify equality; a zero upper norm separately proves the ordered
result is positive zero. Nonfinite operands and nonfinite publication retain
fail-closed semantics. Direct BF16 endpoint certification is insufficient:
the intervening F32 rounding must be preserved.

The obligation is bitwise F32 equality to the ordered oracle, including
rounding ties, severe cancellation, subnormals, partial row populations and
exceptional refusal. Kernel/build identity distinguishes the implementation;
artifact and binding contents are unchanged. Component evidence is not
upstream model conformance or full-model quality qualification. Component
oracles and the bounded complete-model generation/lifecycle gate pass at the
[documented integration snapshot](../evaluation/retained-observations.md#competitive-computational-integration-2026-10-07).
Current broader qualification state belongs to the selected competitive Task
and its independent Evaluation planes.

## Internal Activation-Prefill Boundary

`include/yvex/internal/runtime_prefill.h` owns the non-installed production
contract for activation-driven attention prefill. Schema v1 binds the logical
model, runtime numeric, runtime descriptor, attention plan, operation scope,
token range, all 43 ordered layer identities, exact widths and strides,
canonical little-endian F32 payload ranges, payload digest, and input identity.
It serializes fields explicitly and never hashes native structures, pointers,
paths, padding, or timestamps.

`yvex_runtime_activation_input_open_memory` and
`yvex_runtime_activation_input_open_file` project the same immutable facts.
The file adapter retains a read-only regular-file handle and bounded mapping,
rejects symlinks, duplicate/missing/reordered layers, invalid dimensions,
overlap, truncation, trailing bytes, digest mismatch, non-finite payload,
resource overflow, and file drift. Close is idempotent.

`yvex_runtime_activation_prefill_execute` admits position and capacity before
mutation, divides the activation range into deterministic chunks, and invokes
the shared production attention executor for every layer. Each successful
chunk commits one complete persistent-state generation and advances position
once. Failure or cancellation aborts the failing chunk while preserving the
exact earlier committed prefix. CPU and CUDA eager consume the same activation
contract and session-owned provider; CUDA never falls back to CPU.
Because this input contract contains activations rather than token IDs, the
attention-state owner seals each exact finite F32 activation row in position
order. Its committed identity is independent of chunk boundaries; it neither
guesses token IDs nor substitutes one execution identity for an entire chunk.

The result publishes activation-input identity, chunk/layer/class counts,
attention-output digest, persistent-state digest, committed prefix, position
and generation transitions, and execution identity. It is not a complete
transformer hidden state. Prompt text, tokenization, embedding, FFN/MoE,
cross-layer transformer composition, model decode, and generation remain
outside this API.
The persistent-state digest authenticates admitted logical input lineage and
committed extents; equality across backends does not by itself prove bytewise
equality of their numerical state. Output comparison is a separate numerical
gate.


## Internal MoE Execution Boundary

`include/yvex/internal/moe.h` owns the non-installed MoE plan, typed input,
generic graph/backend execution packets, inspect context, and operator result.
The plan imports immutable runtime descriptor and materialization facts for all
layers; family policy enters through the explicit compiler-facing graph adapter
and is sealed into the pointer-free plan before runtime model-open. Generic MoE
compilation does not discover concrete families through a global registry.

`yvex_moe_input_open_memory` and `yvex_moe_input_open_file` admit the same
schema-v1 identity chain. The bounded file form stores explicit little-endian
header and layer records followed by finite F32 activations and numeric U32
token IDs. Token IDs are routing input only. The adapter rejects unsafe files,
stale identities, malformed layer order or geometry, invalid ranges, payload
digest mismatch, non-finite values, drift, and resource overflow.

`yvex_runtime_moe_context_open` seals reusable session resources and returns
the exact CUDA workspace requirement derived from the admitted layer geometry
and maximum row width. `yvex_runtime_moe_execute_layer` is the retained
token-local CPU/CUDA oracle; it accepts one expanded hidden activation and
returns distinct router, routed, shared, combined, and deferred mHC post facts.
`yvex_runtime_moe_rows` is the ordered width-N execute/complete contract.
Full-stack production CUDA routes the complete row set, constructs one
deterministic expert-major pair order, executes grouped routed and shared paths,
and defers publication across its layers. It transfers only bounded status and
unique-expert facts, validates them once at stack completion, and derives exact
active bytes without materializing selected routes. Portable, audit, forensic
and standalone block execution retain a completion-safe immediate oracle.
`yvex_runtime_moe_execute` executes an ordered all-layer input and publishes
only after the complete request succeeds. Reset and close preserve session
isolation and never advance persistent KV or sequence position.

The CPU and CUDA backends consume `yvex_moe_layer_job`. The retained oracle
executes exact selected routed-expert subviews plus the separate shared expert;
the production CUDA row operation consumes resident complete expert packs
through a private capability table. CUDA performs all numerical stages on
device and has no CPU fallback. `yvex_moe_operator_result` is a copied result
surface; it is not capability authority and is not a transformer or generation
result.


## Internal Transformer Execution Boundary

`include/yvex/internal/transformer.h` owns the non-installed transformer plan,
canonical numeric token input, reusable execution context, single-block
completion, full-stack coordination, and copied operator result. Schema-v1
plans bind exact runtime, attention, MoE, embedding, mHC, and final-norm facts;
schema-v1 inputs bind canonical U32 token IDs and their model/runtime/plan
identities. Memory and bounded-file admission share one validation contract.

`yvex_runtime_transformer_execute_block` consumes the attention publication
already staged inside an active request transaction, executes the admitted MoE
layer and deferred FFN mHC post, and returns field-wise routing/output/execution
identities. The full-stack coordinator alone selects deferred device-native MoE;
a standalone block remains complete before it returns.
`yvex_runtime_transformer_execute` owns chunk planning, selected-row embedding,
43 ordered blocks, final mHC collapse, final RMSNorm, atomic state commit, and
normalized-hidden publication. Its request carries an explicit
prefill or decode phase; one-token geometry alone never selects decode
semantics. The repeated-decode owner reuses this exact component.

CPU and CUDA share the typed contracts. The GB10 CUDA path retains inter-layer
activations on device and has no CPU numerical fallback. The API publishes
normalized hidden state only; tokenizer text, output-head projection, logits,
sampling, and generation remain outside it.


## Internal Repeated Decode Boundary

`include/yvex/internal/decode.h` owns the non-installed teacher-forced decode
coordinator. It borrows one already-open transformer context and its exact
execution session; it does not reopen the artifact or binding, rebuild plans,
or allocate a second KV owner. Inputs are bounded one-token views of the
existing schema-v1 transformer token input.

`yvex_runtime_decode_step` validates the authoritative committed position,
executes the production transformer in explicit decode phase, and publishes
one `[1,4096]` normalized hidden row only after that token's KV commit.
`yvex_runtime_decode_execute` repeats the same operation over ordered external
token IDs. Each successful step is independently durable; a later refusal
returns a non-success status with the completed step directory, final committed
prefix, generation, and first incomplete ordinal intact.

Step and aggregate identities serialize typed fields individually, including
the phase-bearing transformer identity, token, position, generation, routing,
hidden, persistent state, and structural counters. They exclude pointers,
padding, native object layout, timing, and local paths. The coordinator owns no
token-choice, logits, sampling, tokenizer, or generation policy.


## Internal Vocabulary-Logits Boundary

`include/yvex/internal/logits.h` owns the non-installed, family-neutral output-
head plan, authenticated normalized-hidden source, reusable logits context,
single-row projection, ordered repeated projection, typed results, and operator
adapter. The context borrows one runtime model/session and transformer plan. It
shares immutable output-head residency but owns mutable host/device logits
workspace and concurrency exclusion.

`yvex_runtime_logits_source_from_transformer` and
`yvex_runtime_logits_source_from_decode` seal only producer-authenticated final-
prefill or decode hidden rows. `yvex_runtime_logits_project` computes every
vocabulary coordinate directly from the resident encoded output head and
publishes the caller-owned row only after complete success.
`yvex_runtime_logits_execute` preserves earlier complete rows on a later
failure and records the exact first incomplete row. On CUDA it groups a
compatible width-N directory when host rows form one bounded batch or device
rows form one contiguous identity-compatible view. The group performs one
activation preparation, one encoded-head execution and one ordered output
transfer; its result owns the aggregate physical facts. Mixed, non-contiguous
or invalid sources take the explicit row-local path, preserving the same
complete-row publication and failure contract.

The logits API publishes raw F32 values and field-wise plan, source, residency,
backend, row, and aggregate identities. It neither repeats final norm nor owns
persistent state, sampling, tokenizer, or generation policy.

`include/yvex/internal/decision_readout.h` owns internal Decision Readout schema
v1. It accepts exact token-domain finite candidates over one captured common
runtime prefix and publishes raw candidate log-likelihood, separately named
mean token log-probability, and an optional uncalibrated relative candidate
distribution. Its identities authenticate the admitted model/artifact/binding,
engine generation, tokenizer, compiled forward/output programs, shared prefix,
ordered opaque candidate population, score policy and result. Resource facts
keep mapped package, prepared storage, host/device residency, shared/candidate
state, workspace and logits buffers separate. The header is non-installed: it
does not establish a public C ABI, wire route, sampling policy, calibration,
semantic Decision authority or application ranking contract.


## Internal Real-Logits Sampling Boundary

`include/yvex/internal/sampling.h` owns the non-installed family-neutral
sampling policy, complete-logits source, reusable fixed-workspace context,
single-row selection, ordered repeated selection, typed results, and operator
adapter. The context copies the immutable output-head plan identity and owns
only candidate/probability workspace, private versioned RNG state, counters,
and concurrency exclusion. It borrows complete caller-owned logits and has no
model, session, artifact, KV, transformer, decode, or tokenizer ownership.

`yvex_runtime_sampling_source_from_logits` revalidates a completed logits-row
identity, full vocabulary extent, canonical raw digest, finite values, source
phase and position, hidden digest, and output-head plan. Greedy selection scans
the complete row and resolves exact ties to the lowest token ID without RNG.
Stochastic selection uses the schema-v1 API with filter-order v2: compensated
normalization removes exact zero mass before entropy-bearing filters, then one
PCG-XSH-RR 64/32 transition commits only after complete token/evidence
publication. Result validation authenticates every authoritative evidence
field. Atomic close admission drains active use before workspace release.
`yvex_runtime_sampling_execute` preserves completed earlier rows and the exact
committed RNG state when a later row refuses or is cancelled.

Sampling result identities bind the source, policy, ordered survivor IDs,
selected token, canonical probability, and applicable RNG states field by
field. The API does not mutate logits or persistent state and does not append,
decode, tokenize, stop, detokenize, or generate.

CUDA stochastic filtering evaluates independent F64 exponentials across the
block, while retaining source-ordered compensated F64 normalization and the
existing filter/acceptance order. Cooperative tiled loads feed the literal
ordered compensated fallback. A separately bounded equivalent realization may
avoid it only when a certificate proves identical binary64 rounding; it does
not admit an unordered probability sum with a tolerance.
Independent divisions retain the same F64 divisor and the verification sum
uses the same source order. Stable in-place compaction saves each complete tile
before writing its retained records and preserves token order across tiles.
Initialization retains source-ordered maxima, including signed-zero ties.
Sorting uses the power-of-two extent of the
current survivor population, not the original padded vocabulary. The final
categorical draw omits token sorting only when no rank-based filter could have
reordered initialization/positive-mass/min-p compaction. Tie-breaking remains
lowest token ID. Neither RNG draws, probability publication, bounded result
transfers nor transactional commit change. These execution choices are generic
and do not select a family, model, prompt or transport-specific sampling policy.

### Binary64 compensated-sum certificate

For finite nonnegative probabilities and `1024 <= n <= 2^24`, 256 threads form
compensated pairs over disjoint populations, then one thread merges their 512
components with the same error-free addition/correction primitive. The final
rounded pair is not automatically authoritative. A conservative enclosure must
prove equality to the original source-ordered compensated result.

Let `u = 2^-53`, `A = sum(abs(p_i))` and `gamma_n = n*u/(1-n*u)`.
The error-free residual identity bounds the pre-final-rounding error of one
compensated pair by `gamma_n^2 * A`; see the derivation preceding Proposition
4.5 and equation 4.11 in [Ogita, Rump and Oishi, Accurate Sum and Dot Product](https://www.tuhh.de/ti3/paper/rump/OgRuOi05.pdf).
This is a bound on the pair before rounding, not permission to change the
published sum. Each partial pair has at most n terms. Its components have
combined absolute mass less than `2*A` in the admitted range. The merge has
512 terms, so both partial and merge errors plus the literal pair error are
bounded by `16*n^2*u^2*A`, using `gamma_n <= 2*n*u`.

For this nonnegative population, the merge's high component H satisfies
`A <= 2*H`: the partial-pair error and the merge's ordinary summation error are
less than half A in the admitted range. The implementation conservatively
encloses the difference between pairs with `64*n^2*u^2*(2*H)`, rounding its
bound upward. This deliberately exceeds the derived bound. Only finite bounds,
a normal positive rounded result and `H >= abs(L)` are eligible. Error-free
FastTwoSum recovers the exact residual of rounding `H+L`. Directed subtraction
and addition enclose the residual minus/plus the upward bound. Certification
requires these endpoints to lie strictly inside the lower and upper half-gaps
of the rounded value's binary64 rounding cell respectively. The gaps can differ
at powers of two; a symmetric smaller-gap test is unnecessarily conservative.
Thus both pair estimates lie in the same rounding cell, including the original
ordered pair. Midpoints, exceptional inputs,
underflow-range results and unsupported dimensions retain literal evaluation.

Add/subtract error-free transformations retain gradual-underflow semantics;
probability exponentiation, division, filtering, categorical draws and RNG are
unchanged. The numerical obligation is bitwise equality to the literal ordered
binary64 result, not agreement with a differently rounded exact real sum.
Independent literal and exact-arithmetic fixtures must exercise certified and
fallback cases before this realization is accepted; a faster model request
alone is insufficient.


## Internal DeepSeek Attention Operator Boundary

`yvex_graph_attention_operator_execute` is the non-installed typed adapter used
by the offline `yvex bench attention ...` lane. Inspection and profiling of
the same owner use the `yvex inspect attention ...` and
`yvex bench attention ...` projections. The adapter consumes a runtime
binding, common runtime model/session, admitted external artifact, and either a
canonical diagnostic probe or admitted tensor-file activation input. It never
calls Make, a test executable, another process or the test-only oracle.

The operator distinguishes:

- attention `prefill`: a multi-token activation chunk with an immutable prior
  attention-state view;
- attention `decode`: one activation token with an immutable prior state view;
- mixed and speculative phases: represented but refused;
- attention core, attention envelope and complete release-attention-set scopes.

These phase names do not mean tokenizer-backed prompt prefill or model decode.
The same operator may consume and publish the session-owned persistent
attention-state provider. Embedding, full-model prefill, MoE, transformer
composition, logits, sampling and generation remain outside this API.

CPU admits eager execution. CUDA admits eager, piecewise CUDA Graph and full
CUDA Graph execution plus an `auto` dispatcher. Explicit mode requests either
run that mode or refuse; only `auto` may select another admitted mode and must
report why. CUDA execution does not fall back to CPU numerical work.

The runtime returns four different identities:

| Field | Hashes |
| --- | --- |
| `tensor_output_digest` | canonical output tensor geometry and bytes |
| `state_delta_digest` | canonical candidate attention-state delta |
| `execution_evidence_digest` | backend/mode-specific stages, graph facts and counters |
| `execution_identity` | complete request/result compatibility contract |

CPU and CUDA expose separate exact output and state-delta digests. Equal bytes
produce a common digest; when exact bytes differ, the common field is
unavailable even if the versioned numerical comparison passes. The comparison
reports output/state value counts, finite and non-finite counts, first failing
stage and coordinate, maximum absolute/relative error, RMSE, and separate
byte-equality facts. Its state lane covers raw KV, compressed/indexer emissions
and positions, and both rolling-state components. Evidence digests remain
backend and execution-mode specific.
