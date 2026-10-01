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

**Identity-bound execution observations and their limits.**

[Up](README.md)

The undated observations below were retained at the pre-migration source
[`b5f632ef`](https://github.com/yailabs/yvex/tree/b5f632ef8d452f61bbbc8f91f9459a2fd26b7c83).
They are not rerun by documentation validation. Scope and missing provenance
remain visible; [Status](../project-control/STATUS.md) alone owns current maturity.

## Native interface composition (2026-09-30)

`INTERFACES.CLI.REPLAI.PRODUCT.SURFACE.REFOUNDATION.0` qualifies a shared human
presentation/interaction producer without changing model execution or the private
wire. [ADR 0007](../decisions/0007-external-terminal-editor.md) owns the split;
the [command reference](../reference/commands.md) owns registry/product grammar.
YVEX owns typed facts, exact channels and admission. REPLAI owns cell geometry,
semantic documents/styles, menu mechanics and terminal/output restoration.

### Immutable authority

YVEX started at `5d84349f8774a4273ee5f4ac4ce3a05ce99c64e1`, tree
`26925f595b733d7a07939a926f97721de1757e4c`. Its final implementation receipt is
`0ae97043e36f6dc633e7820c4cedf12003108486ccab3fcb9667612d74d0cb55`, source delta
`35029aa1f7237f39ba0d59eb692323f813abb45ad39e6b3af29743f50762043b`, build identity
`a200518beff13d090824573f6e36eb8fef81ecc8509430a349e0a026cbdc3d09`, executable
`bc31b78a640943c524fc606d2040ffd861c0ca98da6a3ed148db7cc8780f6ff2`.
The mapped run froze that source throughout: 121 PASS, zero FAIL, ERROR, BLOCKED
or SKIP, with `source_stability.valid=true`. Published test-oracle corrections
below do not change those production inputs. Final closure edits affect only
documentation/project control and receive separate documentation validation.
Raw receipts and PTY captures remain outside tracked payloads.

The first hosted CI run at `356d924c` reports 116 PASS and two FAIL. The
documentation surface checker still called the old root executable instead of
`YVEX_BIN` and required flat advanced-help lines; that local pass did not qualify
the new help projection. The repaired checker consumes the selected binary and
joins parent/leaf syntax, with negative command/argument identity controls.
The source-acquisition failure had no assertion context. Independently holding
the legitimate provider-partial-before-metadata stage reproduces its existing
`completed_files == 4` assertion failure. Qualification now waits for one exact
snapshot containing both facts and deliberately exercises that early stage;
it neither changes progress semantics nor relaxes the expected values. This
demonstrated test race is not retroactively asserted as the unlogged CI cause.

The repaired published source
`e5c0e550879c990100587d1bd6f5d989cb39530e`, tree
`03252c399f6f897bfeb256b62067a899ac8819ee`, passes the independent
[hosted qualification](https://github.com/yailabs/yvex/actions/runs/36792686079):
118 PASS, zero FAIL/SKIP/BLOCKED/ERROR, receipt
`fbde941b75000ba6efec1880bee61f9cf04c99ef28bd1b2fc3d4978932bd2c51`.
All ten separately checked chat processes report zero Valgrind errors,
zero bytes/blocks in use at exit and no suppression. Clean local confirmation
also passes the selected-binary documentation surface and source-acquisition
lanes; the latter deliberately observes the early partial stage before its
completed metadata. This closes the interface publication gate, not the separate
model/release gates. Subsequent documentation-only closeout retains identical
production, test, registry and build inputs.

REPLAI started at `5c8594923f8153de347ad6f8a96d0db8382080ee`.
Independent producer qualification used
`19845f5ae24fc0b9589a2621d03a8d1ba1ad47c2`, tree
`1e012f04a871bf65c9f05bfb16659419d000eac6`, Rust source subtree
`7b62ec3136df94a9f906307d2e1e9276ca19530a`.
The subsequent published pin
`93d62f6d34cfb933a1f59407ade027152e1ef2ba`, tree
`02e946c6ac5b3baf746cff9eade7f10ba7ed5669`, has identical implementation, ABI,
header and Cargo inputs. Its archive SHA-256 is
`a3095b82e53f55067105820f66ffe3fe003d274eec69e91b44186692712c9ee1`.
Base C ABI 1 is unchanged; presentation extension 1 is queried separately.
The consumer downloads and verifies this immutable producer, not a sibling tree.

### Composition and controls

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| REPLAI producer | Independent clean checkout; native qualification and packaging drivers | Rust, C/C++, static/shared and moved-prefix consumers; PTY/misuse/memory controls | Preserve ABI 1; bounded P1; restoration and refusal | 22 qualification gates and 27 software packaging phases pass; pinned-revision CI passes | Exact layouts, bytes, termios and zero memory-checker errors | PASS | Independent Linux producer, not customer distribution readiness |
| Registry/CLI | Canonical registry and generated projections | 175 operation dispositions; positional/flag enums/cardinality; compatibility aliases; version JSON | Parser/help/discovery/completion agree; malformed requests refuse before dispatch | Mapped acceptance and negative controls pass; all 49 former opaque argument packs gain exact static grammar | Exact metadata and typed exit/schema identities | PASS | Command grammar, not runtime support inferred from a name |
| Human terminal | Actual old/new executable captures and read-only resident facts | 60 before/after CLI captures; 30 live host reads; 40/80/180 columns, styled/plain | Sparse records/groups; no dropped facts, boxes or plain ANSI | Model cards, grouped advanced help, responsive engine/host records and errors inspected | Cell-bounded lines and unchanged terminal attributes | PASS | Human layout at these widths/modes |
| Chat PTY | Production client against isolated protocol fixtures | Startup/help, slash/context menus, stale tickets, Unicode/paste/history, progress, exact channels, disconnect/cancel/resize | Preserve input/channel meaning; clear transient feedback; restore prompt/terminal | Mapped PTY suite and repeated-turn controls pass | Exact protocol bytes; balanced paste; termios before equals after; 5 TTY descriptors across scopes | PASS | Interactive composition and cleanup, not model behavior |
| Real CPU vertical | Compiled bounded decoder and normal resident host | Input `a`, two turns through chat, reset, checkpoint/fork and stale/cancel/refusal controls | `okokok`, committed position 5, next prompt and independent subsequent work | Exact response/state and lifecycle controls pass | Exact fixture result; no upstream-model equivalence claimed | PASS | Actual generation through the normal host, not a mocked completion |
| Host memory safety | ASan/LSan, UBSan and unsuppressed Valgrind | Changed CLI/host composition; four real chat processes and 20 repeated edit/generation turns | No invalid access/leak/undefined behavior; failure cleanup | Both mapped sanitizer lanes pass; all four checked chat processes exit with 0 errors and 0 bytes in use | Zero errors/leaks; no new suppression | PASS | Software memory/lifecycle safety at exercised scope |
| Machine/API | Existing serializers and protocol/API acceptance | JSON acquisition success/dry-run/failure; host/session/engine/OpenAI/remote fixtures | ANSI-free structured output; existing schemas and routing retained | Mapped integration lanes pass; acquisition stdout is one typed result, not parsed supervisor logs | Exact JSON/schema/status controls; local protocol remains v24 | PASS | Machine separation and preserved existing API meaning |
| Documentation | Native metadata/link/project-control and publication checks | 103 owners, architecture diagram, contracts, reference and ADR | Coherent owners/links/counts; common Markdown → HTML/PDF | Structural gates, HTML and browser-backed PDF generation pass | No runtime maturity inferred from publication | PASS | Documentation/publication integrity |

The real CPU package is artifact
`a946a8447534b15556e9b6d38c57cfee2bb3f5f05ffa5932ac9c672b7641341d`, binding
`5911a93e48dbe4b993b13fae4b3e2773f688db02ec125de92227f145f41c9884`, context 8.
No model weights or operator payloads become repository fixtures.

Thirty-two installed static/shared producer PTY cycles observe explicit resize
advance at 0.002–0.003 ms and Esc visual dismissal/advance plus drain at
0.012–0.139 ms. These are local call observations, not end-to-end event-delivery
SLAs. The original 250 ms fragmented-sequence deadline remains intact; visual
dismissal no longer waits for it. YVEX uses producer readiness/deadline facts
and a resize wake rather than the former 100 ms editor poll cadence.

### Nonclaims and live-service boundary

Wrapping is Unicode/grapheme-cell based, not language-aware word wrapping or a
universal terminal-emulator guarantee. Extension qualification is native Linux;
retained base macOS evidence does not qualify the new extension there. Plain
and styled output carry the same facts; no alternate screen, opaque background,
caller ANSI injection, domain meaning in REPLAI or fallback editor is introduced.

The existing production host was only inspected. PID 3951411 retained loaded
and on-disk executable digest
`2999dc940a7cdfd9cfb214c47b29af20bc2813425e0357327ab2b8a7340db4e7`;
its attached user session was not changed. The candidate is built under
`build/interface-refoundation/`, not installed over that live executable.
This does not requalify DeepSeek numerics/performance or the YAI product chain.
The separate legacy `cuda.native` artifact blocker and broader retained
numerical failures are not erased by the interface selection's 121 passes.

YAI, Studio and SDK are untouched. API-only finite-decision discovery does not
implement Laya lifecycle convergence. Remote management remains read-only v1;
A03 remains READY. Static-link notices and receipts retain MIT attribution, but
exact recipient legal/package closure remains UNQUALIFIED and fail-closed.
No release, customer distribution or unrelated capability promotion is earned.

## DeepSeek GB10 inference pipeline (2026-09-30)

`V010.RUNTIME.DEEPSEEK.GB10.INFERENCE.PIPELINE.1` preserves the preceding
ordered-dot launch-geometry gain. Its fresh profile identifies three further
generic costs: arenas sized by logical prefill rather than executable rows,
duplicated paired-input traversal, and storage dispatch inside ordered dots.
The [structured observation](benchmarks/data/deepseek-gb10-inference-pipeline.json)
retains exact variant/executable identities, samples, request/output hashes,
strategy comparison and instrumented cycle economics; its
[generated view](benchmarks/generated/deepseek-gb10-inference-pipeline.md) is a
projection, not another measurement owner.

### Authority and comparable controls

The later user-authorized official-vector import is a separate input/reference
claim inside this delivery: the pinned DSpark snapshot contains four official
encoding/parsing input/gold cases. Twelve file hashes (including tokenizer and
upstream license) are retained in the test-vector manifest. The unmodified
upstream test passes four cases; native BPE encode/decode passes all four gold
strings, and one supported request prefix is independently byte-exact. Thirteen
text and three simple prompt comparisons also pass. The native proof uses the
authenticated runtime-binding tokenizer, not the unbound legacy CLI inspector.
Complete native transcript projection and official full-model logits remain
NOT QUALIFIED/NOT RUN. Upstream payloads and MIT notices remain external; the
repository imports identities and a reproducible test adapter, not model data.
The final registered receipt is `5af92326…` (61.082 seconds); injected missing
and corrupt backing both refuse before upstream or native execution.

Baseline source is `115884e6970676df66c587ad359563bf047773a2`, tree
`6f8787db8551eabb79358b3c015c9673a4538bdd`, executable
`00064493ee9c3c19bf9f1545a43a2cf64815512501d825bb59ddcf085ea2868e`.
The isolated changes culminate in measured executable
`77f0828892b5b752f5c3ab1ef92bdf2946b14dff8fa8f7e0b7bf3b4008498d32`,
tracked source delta `79894e2b945f47f4eaa6202efa07d6ae8b07a05de21efce22bdbc25f776158cb`.
The subsequent reviewed-build confirmation is
`ffda5718a7af89c9e16062ed7a8e29d6ed0c95939d57c6d76ed1a884cafba2bd`,
complete compiled delta `fffaf0177855f1ced7025e16d33e47e8f4551c693e390f12fbb7d86b33a817b4`,
build identity `6235d951d43080317774b809782a5dc8c9de5733096ce7e510381d95e2ee88f8`.
Each run freezes source; the reviewed live `/proc/PID/exe` equals its built
executable. Raw receipts remain outside Git under
`/home/dgmothx/lab/models/evidence/yvex-gb10-pipeline-20260930.z5UwYo`.

All controls use the same admitted mixed DeepSeek/DSpark artifact, binding,
runtime model, specialization, GB10 UUID and driver listed in the producer
reconciliation below. Context is 32,768, configured prefill 64, one worker,
temperature zero, fresh zero-prefix ephemeral sessions and warm mapped weights.
No operator Case content or retry is used. Samples have no collecting profiler.
The fresh baseline has one sample per suite member; candidate/strategy controls
have three. The preceding clean baseline separately retains three stable A
samples (about 8.1 seconds HTTP). These are bounded observations, not an SLA.

| Fixed control | Rendered input / committed output | Baseline HTTP s (n=1) | Optimized HTTP mean s (n=3) | Optimized range s |
| --- | --- | ---: | ---: | --- |
| A, exact `YAI_OK`, non-thinking | 22 / 3 | 8.131 | 5.377 | 5.326–5.449 |
| B, bounded integer continuation | 20 / 32 | 16.501 | 12.212 | 12.132–12.264 |
| C, source-authored high reasoning | 17 / 41 | 21.245 | 16.078 | 16.014–16.117 |
| D, source-authored maximum reasoning | 96 / 58 | 39.922 | 31.662 | 31.636–31.693 |
| E, modest larger prefill | 100 / 3 | 21.517 | 16.430 | 16.393–16.456 |

The reviewed build independently confirms A at 5.247 seconds HTTP mean
(5.193–5.336), 3.456 seconds prefill and 4.179 seconds turn-relative first token,
three samples. This confirms preservation through the Makefile review; it does
not establish an additional optimization beyond ordinary sample variation.

### Dominant owners and changes

Nsight Systems graph-node traces are diagnostic runs, separate from wall-time
comparisons. The baseline's one `cuMemHostAlloc` and one `cuMemFreeHost` cost
1.988 and 0.931 seconds; after canonical arena sizing they cost about 0.549 and
0.270 seconds. These lifetimes explain substantial time outside the generation
turn, rather than tokenizer or transport latency. The same capacity owner now
derives physical rows from admitted scheduler width and exact source proposal
staging. The current target uses six rows; DSpark requires seven. Transformer,
decoder, MoE and draft consume this one fact instead of independently allocating
64 configured rows/global maximum draft rows. Pre-engine admission stays
conservative. Logical context/chunk, routing and real executed populations are
unchanged; no arena cache, pool or borrowed-lifetime shortcut is introduced.

Capacity alone changes A HTTP to 6.181 seconds, with prefill unchanged near
4.14 seconds. Paired BF16 traversal sharing changes it to 5.843; hoisting
storage-invariant ordered-dot dispatch changes it to 5.377. The paired kernel
profile falls from 1.324 to 1.047 seconds; ordinary qtype matvec falls from
1.798 to 1.267 seconds. The final profile is still dominated by ordered decoded
projection: matvec 35.6%, paired 29.4%, grouped rows 15.6% of recorded GPU kernel
time. Device/host synchronize API durations include waits for this same GPU
work and must not be added to it. The A trace has 13,905 direct launches,
454 graph launches, 362 graph instantiations, 5,207 context synchronizations
and 2,467 stream synchronizations. Direct launch API time is about 20 ms:
launch count alone does not establish the dominant cost.

Each paired dot retains an independent source-order F64 accumulator and original
F32/BF16 cast. Storage dispatch selects an equivalent typed decode loop once per
row; other representations retain the previous generic path. Precision,
experts, routing, attention/mHC, validation and target verification do not change.
A packed narrow-F32 thread trial passed its component control but did not
materially improve complete requests (A 5.415 seconds, E 16.285); it was reverted.

### First token, decode, reasoning and speculation

One reviewed A SSE sample resolves host-observed phases from request receipt:
session created 0.030 s; turn/prefill started 0.660 s; prefill completed 4.113 s;
first committed token 4.796 s; session closed 5.203 s; complete HTTP 5.204 s.
Turn-relative first token is 4.136 s, not HTTP-visible TTFT. Mean external first
fragment across the three samples is 4.796 s, versus baseline 7.072 s.
This separates preparation, actual prefill, first decode and teardown without
moving excluded work out of the complete-request metric.

Bounded B post-first publication is 4.862 committed tokens/s across 31 tokens,
versus baseline 3.977. This is bounded decode characterization, not sustained
serving throughput. High and maximum first reasoning fragments change from
7.229/20.598 to 4.888/15.953 seconds; prefill changes from 3.257/16.465 to
2.787/13.852 seconds. Authored maximum rendering really produces 96 input tokens;
it is not the same rendered geometry as high. No effort, source instruction,
reasoning token, stop or channel classification is removed.

Both modes naturally reach `</think>` and final `4`. DSpark can publish the
terminator and first final token in the same verified cycle: observed final
fragment gaps are microseconds, not proof of an isolated zero-cost transition.
Target-only gaps are about 0.777/0.780 seconds. A separate high-mode F control
(three samples, 20 input / 62 output) naturally emits reasoning followed by
`2 plus 2 equals 4.`, completing in 21.853 seconds. This establishes a real
multi-token final continuation, not a forced terminator or general transition SLA.

The explicitly selected target-only engine uses the same executable, artifact,
binding and context, after sequential unload/load; no hidden strategy policy
is added. Three-sample HTTP means for target-only versus DSpark are A SSE
5.712/5.278, B 16.717/12.212, high 19.795/16.078, maximum 37.378/31.662 seconds.
Target-only publishes the first fragment earlier (A 4.506 versus 4.862 seconds),
while DSpark finishes these controls sooner. Thus complete-turn benefit does
not imply improved first-token latency for every strategy.

Full-trace diagnostics record B five cycles, 25 proposed / 20 accepted / 5
rejected; high nine cycles, 45 / 24 / 21; maximum twelve cycles, 60 / 35 / 25.
Mean accepted prefixes are 4.0, 2.67 and 2.92 (maximum five). Draft/verification/
commit timings are retained separately; commit includes target correction work,
and full tracing perturbs timings. They are not substituted for ordinary samples.

### Qualification, resource truth and next limit

Paired projections are compared bit-exactly with ordinary CUDA and independent
host decoded scalar F64 dots at width 4,096, including row tails. Existing
non-finite/overflow and quantized class controls remain required. Reviewed live
controls return HTTP 200 with identical bounded output; stale generation returns
409, oversized output 413, disconnect cancels, and independent/replacement work
succeeds. Every checked idle state has zero active work, sessions, leases and
physical session state. The final mapped QA ledger separately classifies absent
mandatory external assets; no missing gate becomes PASS.

The source-stable mapped campaign `981a55af…` ran 161 required assertions:
134 PASS, 8 FAIL, 19 BLOCKED, zero SKIP/ERROR. Three structural failures shared
one stale scanner of the former monolithic Makefile; the repaired structural
lane `19ccccf0…` passes all 17 assertions. The strengthened ordered-dot device
oracle passes (`5cb277b4…`), and registered official input vectors pass
(`5af92326…`). The later fast/structural receipts `455c24ec…`/`6078fd23…`
separately pass 97/17 assertions. These are source-bound receipts, not one
summed gate.
The old client fixture also lacked the immutable artifact hash and assumed an
explicit device copy, obsolete UI text and telemetry v3; it now checks typed
mapped/addressable ownership, current terminal behavior and telemetry v6.
Its repaired live receipt `99d7251e…` passes in 122.864 seconds, preserving two
KV-reusing turns, exact checkpoint/restore/reset, native PTY completion,
reasoning/final separation, cancellation/recovery and supported host shutdown.
The optimized paired/grouped projection device memcheck reports zero errors;
this bounded control is not the separate full-qtype diagnostic.

Four numerical failures are independently reproduced in a disposable archive
of unchanged `115884e…`: DeepSeek decode, prefill and production transformer,
plus Qwen Decision Readout's independent-score assertion. Baseline and candidate
prefill output/state digests are identical to each other; baseline and candidate
transformer layer/logit diagnostics are identical (first mismatch layer 6,
hidden max absolute error 1.09375, argmax 339/295, probability TV 0.362520746).
These assertions remain FAIL, not reclassified as external missing assets or
fixed by this performance work. The independently admitted forensic CPU/CUDA
control still agrees through all 43 layers and logits exactly: max absolute
error/RMSE/TV zero, argmax 339/339, common hidden digest
`f00d7529f7efaf4f1e92ff1455e868be92514654c4632f6afab287431af43467`.
It does not qualify CPU equivalence for the distinct production Q8 class.
Full mapped-gate closure therefore remains unearned, separately from the
bounded performance, component and producer/lifecycle result. Unconfigured
source/emission or benchmark lanes are NOT RUN; only genuinely unavailable
identities such as legacy bootstrap-Q2 are external asset blockers.

The mapped 95,050,210,272-byte model remains borrowed/device-addressable.
Physical residency is unknown, and RSS is not additional independent model
storage. Typed target active workspace is 3,421,365,264 bytes; after request
retirement typed current workspace/session state is zero. No retained scratch
or preparation cost is hidden elsewhere. Nsight Compute counters were refused
with `ERR_NVGPUCTRPERM`: achieved occupancy/utilization and a hardware floor are
NOT QUALIFIED; no driver configuration was changed to manufacture evidence.

The concrete next physical constraint is not a configured chunk setting.
Specialization derives its maximum row-width mask from the compiled model's
verification width; this DSpark source supplies block five plus one, and
routed admission additionally bounds the generic width mask. The observed
22/100-token prompts therefore use four/seventeen physical prefill groups,
not one 64-row matrix. Wider phase-specific prefill requires compiler and
specialization admission independent of verification geometry. Conventional
parallel reductions/Tensor Core substitution would also need their own admitted
numerical class rather than silently replacing ordered F64. Neither distinct
boundary is implemented or selected here. The remaining ordered projection
cost is measured; inability to improve it locally is not a hardware-limit proof.

The unavailable legacy bootstrap-Q2 attention artifact still blocks its exact
`cuda.native` aggregate. No 17K/32K performance, sustained reasoning, release,
upstream whole-model conformance or Studio/Case-chain claim is promoted. A03
remains READY and unstarted; YAI, SDK and Studio are untouched.

## DeepSeek CUDA producer reconciliation (2026-09-29)

`RUNTIME.CUDA.MOE.NUMERICAL.CORRECTNESS.0` distinguishes a historical service
failure from two reproducible decoded-projection numerical-class defects.
This is bounded producer/component evidence, not whole-model upstream
conformance, long-request qualification or a completed Studio/Case chain.

### Source, executable and admitted model

The clean DGX checkout started on historical `models2` at
`5edb91df87246c3f59262d77182d2276466d88f5` (tree
`df8000eaab3eaf318233bd7c63ad3c011807a828`). Fetch established no unique local
commits; the canonical worktree switched to `main` and fast-forwarded to
`1073450a7f4f167e2718b3f8cba72e7492368f20` (tree
`899aa0a8e227c4b561ac2de98cd32cba6c293cc3`). No foreign work was discarded.

The former service used a deleted executable with SHA-256
`f54647ca1b1f1e4ed95165bb497a6c6c8a95f45c17c8241d55b165e8361e4d27`,
different from the then-on-disk `87652d89988e372e9a265bcb7ecaf8937dc2ca3bcd311273d7e07bb22bf50c97`.
It was stopped through the supported host lifecycle only after authoritative
work, session, client, model-lease and queue counts were zero. That deleted
executable was not retained; its original numerical cause remains unproved.
Layer 30/status 1 identifies an observation site, not an originating kernel or
a CUDA driver error.

A fresh SM121 build **before either kernel repair** had executable SHA-256
`525c0e3aad446a76d530f34ca3b26b7d1fceecaae7f90dbd77114759949ece36`,
build identity `76edf1771c3af0a9c0a5a9571cc25835095c5079427e6d72806007dd0a773d17`
and source delta `1e654643d229d6688310d06c998df45e9da0c92a240cf524ac7e95055b5be34e`
over that main tree (Task documentation only). The loaded `/proc/PID/exe`
digest equalled the built executable. Both the small control and the original
synthetic provider request already completed on this baseline. Consequently,
the kernel changes below are **not** a demonstrated explanation of the former
HTTP 503. No source-level repair of that historical failure is claimed.

The post-repair measured executable was
`47f47df0ed5c2383fba466c855f74fc27407be095bbdf4a87617fa23de8281ab`,
with build identity `8eac22cf9215010c8fe968475ea45564a63ebf1e529d3fb9b38416e6da9e118c`
and source delta `ac93db10a9e402ee7053968a07e18b8d8a17b45916c8745daea0c4a50eefe4c6`.
The loaded executable matched again. Kernel source identities are
`dd9dbec009d5cbb8896146816ba41b7958598fc2bcade59d520f9a917f839a46`
for paired attention and
`d9f10c30ac7a25b35dbeb257e66809d9462bac70e1a860f3a13c15224a904745`
for grouped rows; the corresponding native CUBIN identities are
`d4db307a195a7b4590af23eb148905c460533da83b9cd8c79e62fd9a438e1f40`
and `ee2627420752184dafde1a0a12152b0ba8fa8a0404cd1c99effcd625736cc7f7`.

All HTTP observations use the exact admitted model
`deepseek4-v4-flash-dspark-deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1-cuda`,
engine generation 1, speculative execution and context capacity 32,768:

| Identity | Exact value |
| --- | --- |
| Artifact | `b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f` |
| Runtime binding | `8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e` |
| Runtime model | `cf8ebc69dae8e38f96a47418e8efcebdc47bc4ed05bac76d6974ccc1379e0e7e` |
| Specialization | `7604985ea75253a9338877e3867d2f18daac3be036ffa66babe84aca8ae2554e` |
| Device | NVIDIA GB10, `GPU-7659fe74-b7c2-6e3e-bf99-f66c430366bc`, driver `580.159.03`, SM121 |

### Demonstrated component defects and repair

The existing `cuda.quant_qtype` control first failed its grouped-row bitwise
comparison and, after that repair, its paired-BF16 comparison. Ordinary
decoded-input projection uses source-order F64 accumulation followed by F32
publication. Grouped and paired kernels instead used parallel F32 sums and
recovered only exceptional non-finite sums. Finite rows therefore differed:
recovering overflow did not preserve the ordinary numerical class.

Both generic CUDA kernels now use the existing `qtype_dot_recover_f64()` owner
for every decoded row. The paired kernel's duplicate exceptional-dot loop was
removed. Q8 activation reduction, quantization, expert routing/populations,
SwiGLU, transaction semantics and final F32/BF16 publication are unchanged.
There is no family/layer/prompt branch, disabled validation or CPU fallback.

| Control / authority | Expected | Observed | Tolerance / result | Exact claim |
| --- | --- | --- | --- | --- |
| Grouped rows; ordinary projection plus independent CPU decode/scalar F64 dot | Equal grouped/ordinary values; agreement with decoded oracle | 8 groups × 5 inputs, 640 values, zero bit mismatches | Exact internal comparison; oracle `1e-5 * (1 + abs(reference))`; PASS | Grouping preserves the decoded numerical class |
| Paired BF16; ordinary BF16 projection | Both outputs bit-identical | Existing paired projection assertions pass | Zero bit differences; PASS | Pairing changes launch topology, not dot semantics |
| `cuda.dot_finiteness`; independent decoded F64 arithmetic | Finite cancellation survives; NaN/Inf operands refuse before nonlinear clamp | All 80 admitted cases pass; canaries preserved | Registered arithmetic bounds, exact zero overflow cancellation; PASS | Invalid operands are not converted into admitted finite output |
| `cuda.moe_rows`; CPU decoded weights and scalar F64 dot | Encoded IQ2/Q2 specialized/fallback rows preserve their oracle | Registered normal, overflow, tail and refusal controls pass | Registered per-case bounds; PASS | Expert-row computation and finite validation remain intact |
| Added grouped NaN input | Nonzero device status | Refused, followed by normal work cleanup | Exact status predicate; PASS | Grouped finite validation remains fail-closed |

These are component oracles and internal numerical-class comparisons, not
independent upstream whole-model evidence.

### Real producer and lifecycle controls

The discriminating synthetic request contains exactly two messages:
system `Synthetic YAI provider contract probe. No Case data.` and user
`Return exactly YAI_OK.` The original request has only `model`, `stream:false`
and `messages`; its canonical compact-body SHA-256 is
`933a8e8d95d8947609c30878d49ceb289b1e4683d4ecb2128ed675cd37b4453f`.
The bounded repeated control additionally sets `max_tokens:4`, `temperature:0`
and the exact `yvex_engine_generation` (body SHA-256
`9b78489bc63d49a79f5f012b3dd80180ad7ac5d5533de48e2947e5795c88bb08`).
No operator Case content is used.

| Control / authority | Expected | Post-repair observation | Result / claim |
| --- | --- | --- | --- |
| Small `Reply OK.` producer control | Finite completed response, ≤4 output tokens | HTTP 200; 7 input / 4 output; 17.285 s | PASS; prior small scope preserved |
| Repeated discriminating producer control | Same completed result and usage | Three HTTP 200 responses, `YAI_OK`, 22 input / 3 output; 30.398, 31.098, 31.156 s | PASS; bounded deterministic repeated execution |
| Original synthetic text shape | Completed response, no numerical error | HTTP 200, `YAI_OK`, 22 input / 3 output; 39.401 s | PASS; original synthetic class currently completes |
| Synthetic JSON request | Completed parseable object | HTTP 200; 47 input / 8 output; 68.922 s | PASS; bounded JSON producer completion, not semantic quality |
| Stale engine generation | No completion published | HTTP 409 `incompatible_state` | PASS; exact generation refusal |
| Output limit 32,769 | No completion published | HTTP 413 `output_token_capacity_exceeded` | PASS; capacity refusal, not a claim of zero internal admission work |
| Disconnect during admitted streamed work | Cancellation and retirement | Cancellation counter +1; active work/requests, sessions, clients, leases and queue return to zero; session physical bytes zero | PASS; cleanup observed through typed host/engine/resource owners |
| Independent request after cancellation | Engine remains usable | HTTP 200, `YAI_OK`; 30.934 s | PASS; bounded recovery |

Idle unload also retires mapped artifact bytes and balances model opens/closes
before supported host stop/reload. Public output success alone is not a
numerical oracle: these complete-model controls compose the admitted kernels
and retained fail-closed checks, not an exhaustive intermediate tensor trace.

### Qualification and fixture ownership

The first source-stable mapped run selected 128 tests and returned 126 PASS,
1 FAIL, 1 BLOCKED, 0 SKIP and 0 ERROR. Its receipt is
`64ee4a3bb65b92e66a2c9e1de63a9e49304bc695be32ac776252dee68365d97f`.
The FAIL was `live.deepseek.generation`: its two-session fixture synchronized
request starts before preparation, which did not establish simultaneous ready
operations. It observed no width-two physical population, not invalid model
numerics. Production scheduler deadlines and assertions were not changed.

The fixture now coordinates prefill readiness and bounded turn quanta through
the existing generation-turn/progress APIs. It still requires two actual
session sources, width-two prefill/decode rendezvous, multi-source physical
batches/worklists and exact serial token, text, state and RNG agreement.
Three isolated repeated controls passed before adoption. A peer-failure
control also aborts after committed prefill: cancellation is observed through
advance, mandatory finish and context/session close retire the turn, and zero
generated tokens are published. A 60-second fixture barrier guard is not a
model latency contract.

| Qualification / authority | Expected | Observed | Result / exact claim |
| --- | --- | --- | --- |
| Registered complete `live.deepseek.generation` | CPU control; CUDA target-only and DSpark; repeated seeded output; existing lifecycle, acceptance and CLI assertions | PASS, receipt `869c0f11ad0c5e3559c75f3c7ffe0fdc9fec5d8d17343fad701ac764cbce4309`; source stable | Internal composition/lifecycle regression qualified, not upstream conformance |
| Real two-session fixture | Exact serial semantics and width-two ready populations | Greedy tokens `[223,19,16]`; four width-two rendezvous; 172 multi-source batches, 169 multi-source worklists in the registered replay | PASS; real populations, no timing-based guarantee for arbitrary requests |
| Final fixture with peer failure | Active turn retires; no sampled/committed output token; subsequent controls execute | `peer_failure_cleanup=pass`, same serial tokens/state; two-source batching assertions intact | PASS; fixture failure cannot strand an active generation turn |
| Registered `cuda.quant_qtype` | Grouped and paired exact comparisons plus decoded scalar bounds and refusal | PASS, source-stable receipt `ee09575a6b07612f4e4f8d1d12bcf2b82fc157eb224424911409b7d712e1ec31` | Decoded projection regression qualified independently of the missing legacy aggregate |
| `sanitizer.runtime` and `sanitizer.quant` | ASan/LSan/UBSan host paths refuse faults and retire ownership | Both PASS in the mapped receipt | Host sanitizer scope, not CUDA memory instrumentation |
| CUDA Compute Sanitizer memcheck | No invalid memory access in finite-dot/expert-row controls | Exit zero, `ERROR SUMMARY: 0 errors` | PASS; bounded device memory scope |
| Registered runtime characterization | Three admitted attention execution modes, repeated output/replay and exact provenance | PASS in the mapped receipt; no speedup claim | Current-build characterization, not full-model benchmark or optimization promotion |
| Legacy `cuda.native` aggregate | Exact bootstrap-Q2 artifact available | Missing `DEEPSEEK_ATTENTION_ARTIFACT`; fixture requires 108,285,860,832 bytes, not the current 95,050,210,272-byte mixed artifact | BLOCKED; no substitute or aggregate PASS |

The resolved latest outcomes across the mapped receipt, repaired generation
replay and explicit qtype control are 128 PASS / 1 BLOCKED / 0 remaining FAIL,
SKIP or ERROR (129 distinct test identities), **not one green aggregate run**.
Final fixture/document checks are retained separately. Unchanged numerical
owners retain the exact kernel source/CUBIN identities above; fixture and
documentation edits do not manufacture fresh model or sanitizer evidence.
The Task remains BLOCKED until its mandatory legacy aggregate can run; no
full-gate downstream-safe claim is made.

These timings are characterization only. Restoring the declared decoded dot
class is slower on this workload than the first canonical pre-repair service
(three synthetic controls at 9.458–9.728 s). No performance improvement, SLA,
calibration, arbitrary-prompt correctness or 17,316-token Case qualification is
claimed. Public/persisted schemas and protocol v24 are unchanged; A03 remains
READY and no optimization Task is selected.

Raw build, component and HTTP records, measured executable and kernel payloads
are retained outside Git under the operator evidence directory
`yvex-cuda-moe-20260929.L2WMTQ`. The legacy `cuda.native` aggregate requires the
distinct 108,285,860,832-byte bootstrap-Q2 artifact; the current mixed artifact
is not a substitute. Missing legacy evidence must remain BLOCKED.

**Product handoff:** the exact model at the existing YVEX inference endpoint is
safe to requalify with this synthetic workload through the supported YAI
provider chain. This Task neither retries nor mutates the indeterminate Tech
Infra Case. Studio → SDK → governed YAI → producer → canonical result still
requires its own integration evidence.

## DeepSeek GB10 optimization (2026-09-30)

`V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` resumes the existing performance
Task and earns one bounded material improvement, without changing the admitted
model, ordered decoded-projection numerical class or lifecycle. No YAI Case
request was retried and no cross-repository implementation changed.

### Frozen baseline and candidate

The canonical clean starting source is `a73c887e104d5427ecaf3e03104b365af8dc11fb`,
tree `b3e121b3614223f2759d16c904ac1acd8b4a5a13`. The baseline executable is
`0dfc3a256100d2b66ac48ee98b1045512017026c4c0aaa94b13214b96369a82d`,
build identity `9d7a43bc7b783833ae12a0d7672ffb86866a32b094a3e5241a47509d21bb913c`.
Its compiled source is clean; the measured worktree contains only Task
resumption documentation (delta
`c448359289b7076ffcdd2b886a1afe25b9a2f20ddcc5ad566d6d5c2cc5bc0fdd`).

The candidate executable is
`91215e612e4bd753683e84277b77c6cbcf3d26b35e3f5e11be9a313052f388c2`,
build identity `2d1732de0a798e91e8b95f7fa689ff037803f67ac6d0914742d2a09b986f22f6`.
Compiled and measured source both bind frozen delta
`c092b6a5c97b12b7027cb13dedd502ba69d25eb7269796d449b37f568cd6a66f`
over that base. The changed CUBIN identities are
`d524462115903c9752fb0bbbc2900290bb75fc6168bc48ddd099442f59fdc607`
(`kernels`) and
`bd773a682763f4fe1c171a71780ce2d4724c9d12141fc30e65d49d7da7984271`
(`attention_kernels`). Both loaded service executables were verified against
their built digest. Source/executable stability assertions pass independently
for baseline and candidate.

The exact admitted model, artifact, binding, runtime model, specialization and
GB10/driver identities are unchanged from the preceding reconciliation table.
Context is 32,768, prefill chunk 64, speculative DSpark, one worker and physical
sequence width one. Weights are warm; each request gets a fresh independent
session without prefix reuse. An isolated loopback listener on port 18081
excludes unrelated operator traffic. Earlier contested/incomplete records are
not comparison samples. Profiling primers are also excluded; Nsight collection
is off during the complete-request measurements.

The [structured observation](benchmarks/data/deepseek-gb10-ordered-dots.json)
owns raw sample values, executable/worktree/compiled-source distinctions,
request and output identities. Its [generated projection](benchmarks/generated/deepseek-gb10-ordered-dots.md)
is characterization, not a release benchmark.

### Dominant cost and equivalent implementation

A complete Nsight graph-node trace of the diagnostic control attributes 63.6%
of measured kernel time to grouped decoded projection, 16.0% to paired BF16
projection and 17.8% to ordinary decoded projection. The earlier graph-only
trace omitted graph-node detail and is not used as a complete kernel profile.
The ordered F64 repair assigned one independent result to a warp while its
other lanes returned. The backend now packs independent results into threads,
retaining the same ordered F64 helper, operands and final publication casts.
This is generic CUDA launch geometry, not a family, layer or workload branch.
Q8 reduction, Tensor Core classes and the narrow block-owned F32 class remain
unchanged. No new buffer, cache, preparation or transfer path is introduced.

| Profiled kernel scope | Same launch count | Baseline → candidate kernel seconds | Exact claim |
| --- | ---: | ---: | --- |
| Grouped decoded rows | 233 | 17.232809 → 0.544834 | Independent-result packing removes idle-lane execution |
| Paired BF16 rows | 1,550 | 4.341830 → 1.332993 | Same two ordered dots, denser row ownership |
| Ordinary decoded rows | 2,138 | 4.816032 → 1.803008 | Same row/input results, denser thread ownership |
| MoE up / down | 218 / 218 | 0.217980 / 0.119812 → 0.218412 / 0.120785 | Unchanged expert kernels are not the optimization claim |

These are separate profiled primers, not unprofiled wall-time samples. Nested
host spans overlap and must not be summed. All six comparison workloads retain
the same output/usage, speculative proposed/accepted/discarded/verified counts,
target rows, component/output-head and synchronization populations.

### Equivalent complete requests and resources

The discriminating control uses the two synthetic messages above with exact
generation 1, `max_tokens:4`, `temperature:0`, `stream:false`. Its compact-body
SHA-256 is `332d93a1d8f9578b2183553b0f1664e23f5adf7f6ba28ddd4799358a85c79b5f`.
Every sample returns HTTP 200, `YAI_OK`, 22 input and three committed output
tokens. The output bound and workload were not reduced.

| Metric / oracle | Baseline mean; sample range | Candidate mean; sample range | Expected / observed / claim |
| --- | --- | --- | --- |
| Existing prefill-completed telemetry | 22.186489 s; 22.181654–22.188978 | 4.199542 s; 4.191338–4.209530 | Equivalent token work; 5.28×, 81.1% less wall time; PASS |
| First-token telemetry, clock from turn start | 27.749693 s; 27.749016–27.750599 | 5.053513 s; 5.045794–5.065895 | Same first committed result; 5.49×, 81.8% reduction; PASS |
| External complete HTTP exchange | 30.719407 s; 30.655732–30.777607 | 8.323483 s; 8.050025–8.549817 | Same completed body/usage; 3.69×, 72.9% reduction; PASS |

There are three samples per implementation. HTTP sample standard deviation is
0.061122 s before and 0.253206 s after; the gain is far outside this variability.
These overlapping metrics are not additive. Preparation remains included in
its original scope; first-token telemetry is not HTTP-ingress TTFT.

Single bounded scaling controls preserve their exact bodies/results: 7 input /
4 output takes 17.516645 → 6.778754 s HTTP (7.589747 → 1.884176 s prefill);
45 input / 3 output takes 53.785992 → 12.546556 s HTTP (44.975987 → 8.162392 s
prefill). The old prefill cost is token-dependent, not just fixed admission.
This is not a long-context scaling law.

A 20-input/32-output control completes five speculative cycles and five target
verifications with equal output and populations: HTTP 78.270961 → 17.139047 s;
prefill 20.369170 → 4.003273 s; first token 30.462452 → 5.742320 s. The bounded
decode rate **including the first token** is 0.584075 → 3.312073 committed
tokens/s. Three output tokens in one cycle do not establish sustained decode;
nor does this one 32-output-token sample establish an asymptotic rate.

Mapped model bytes remain 95,050,210,272. The repeated controls' process-RSS
high-water samples remain approximately 100.54–100.57 GB, with less than 2 MB
paired difference. This is not physical GPU residency. Idle current session
physical state, workspace and transient bytes return to zero. Turn-peak
workspace is not retained; idle zero is not a peak-memory measurement. No new
backend allocation appears in the repair, and no cost is moved into a new cache
or model-preparation stage.

### Numerical, lifecycle and mapped evidence

| Lane / authority | Expected | Observed / tolerance | Result / supported claim |
| --- | --- | --- | --- |
| `cuda.quant_qtype`; CPU canonical codec/scalar F64 and ordinary CUDA projection | Grouped values match both references; paired BF16 matches ordinary exactly | 8 × 17 × 9 = 1,224 grouped results, zero bit differences; scalar bound `1e-5 * (1 + abs(reference))`; paired exact comparison passes | PASS; partial tiles and more than eight input rows preserve ordered dots |
| `cuda.dot_finiteness`; decoded arithmetic and canaries | Overflow cancellation finite; NaN/Inf and prior status refuse | All 80 cases pass, exact zero cancellation and canaries preserved | PASS; finite validation not disabled |
| `live.deepseek.generation`; admitted target/DSpark and serial replay | Output/state/RNG and real two-session population/lifecycle controls preserve semantics | Greedy `[223,19,16]`; four width-two rendezvous, 175 multi-source batches and 172 worklists; peer-failure cleanup, mutation/capacity, seeded replay and DSpark cancellation/acceptance pass | PASS; internal model composition/lifecycle, not upstream conformance |
| `live.deepseek.logits`; scalar head reference per backend | Full-vocabulary projection and stale-publication refusal | 387,840 values per lane, CPU-reference/CUDA-reference max_abs=0; 2,068,480 reused projection values max_abs=0; stale rows 3/3 refused without RNG mutation | PASS; head/reuse contract, not whole-model CPU/CUDA equivalence (their hidden-state paths differ) |
| `performance.runtime`; runtime benchmark schema v5 | Eager/piecewise/full evidence and replay validate | Three modes and six charts validate | PASS; registered characterization/publication, not release benchmark |
| HTTP lifecycle; typed host/engine/resource owners | No result on stale/capacity refusal; cancellation retires owned work; replacement rejects old generation | 409/413, cancellation counter +1, all work/request/session/client/lease/queue counts zero, physical session bytes zero; recovery HTTP 200; reload generation 1→2, old generation 409 and new generation HTTP 200 | PASS; failure/retirement and subsequent independent work remain usable |
| `sanitizer.runtime`, `sanitizer.quant` | Host ownership and bounds remain valid | Both ASan/LSan/UBSan lanes pass | PASS; host sanitizer scope |
| Compute Sanitizer memcheck; bounded registered finite/expert controls and existing grouped/paired fixtures | Normal exit and no invalid device accesses | Both bounded runs exit zero with `ERROR SUMMARY: 0 errors`; affected fixture reuses the registered test helpers | PASS; bounded device memory scope |
| Legacy `cuda.native` | Distinct exact bootstrap-Q2 fixture available | Only `DEEPSEEK_ATTENTION_ARTIFACT` remains unavailable | BLOCKED; current mixed artifact is not substituted |

The full optional qtype Compute Sanitizer diagnostic exits 11 after its eight
qtype rows, including when no kernel instrumentation matches. Its cause is not
localized; a printed zero memory-error count without normal application exit
is **not PASS**. Normal registered qtype execution and the bounded affected
grouped/paired and finite/expert memchecks above pass. No tool-bug explanation
or broad device-sanitizer success is inferred.

The source-stable mapped receipt
`aebc312804e00d41b9cb006a3b0daad59d3ccfcca5105d3d5d43a7e85581d370`
has 125 PASS / 4 BLOCKED / 0 FAIL / 0 SKIP / 0 ERROR. Three missing-environment
lanes were re-run with exact current assets: generation
`28e7ed767b0828026a098af16076c18e2aaabd1f2a3b8da8710ea7069a621f33`,
logits `dd7bd7bea6401b9f6da749e80229ef205462e8c5ca9e4a275df6effadacfcbcb`
and runtime characterization
`ee015ff071574a781215a896a7d0b52c27fe4356a4da81ae76cedb7633ebafc7`
all PASS. The configured legacy receipt
`7c7a664f938cbb526e7469827c40c58cac5fc222af18084f771af0172cd848a7`
remains BLOCKED. Resolved latest registered outcomes are **128 PASS / 1 BLOCKED**
across 129 identities, not one green aggregate. Final documentation checks are
separate from the frozen numerical source/CUBIN evidence. The bounded
implementation/measurement exit is earned; the Task's full mandatory gate is
still BLOCKED and cannot support a full-gate downstream-safe claim.

Raw source/build, node-profile, HTTP, lifecycle, sanitizer and QA records remain
outside Git in `yvex-gb10-opt-20260929.IU93gg`; retained baseline/candidate HTTP
SHA-256 values are respectively
`d6fb0eb7410358b9ce83ba2597213ef6514bc53b8ac5134ff37d6d696e84d409` and
`0de7e300ecac0e9a78756ca2c7dbd6867676171199bf0bd1d97e4771305d1fee`.

### Exit and product handoff

One coherent material gain is retained; no further performance work is selected.
Remaining profiled GPU pressure is ordinary decoded projection (41.3%) and
paired BF16 (30.5%). Roughly three seconds of HTTP exchange outside the turn
remain unlocalized. Asymptotic sustained decode, 17K/32K input, model-quality,
SLA and release performance remain NOT MEASURED/unqualified. A03 stays READY.
Public/persisted schemas and local protocol v24 are unchanged; no new structural
owner or ADR is required for equivalent backend launch geometry.

The exact 32,768-context producer and bounded synthetic request are suitable for
a new independently governed YAI integration qualification. This evidence does
not qualify Studio → SDK → Case → YVEX → canonical result, mutate operator
Cases, or resolve an indeterminate prior delivery.

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
