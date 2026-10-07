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

## Rust product-shell qualification (2026-10-03)

`INTERFACES.CLI.RUST.PRODUCT.SHELL.MIGRATION.0` moves operator ownership,
not computational semantics. [ADR 0009](../decisions/0009-rust-product-shell.md)
owns the structural choice. The source authority starts at main
`67a7905ea9deb98b0704629a1f979634e19007fb`, tree
`8734e779c19c78cda6d2612f08062234066e7caf`. One generated registry projects
143 active CLI paths and 19 slash operations. Rust owns parsing, dispatch,
serialization, presentation and native chat; typed C owners supply facts and
retain source, artifact, compiler, runtime and transport meaning.

The retained C reference is an immutable comparison executable, not a fallback.
Compiler-derived FFI records, independent C readers and JSON/CSV differentials
qualify machine boundaries. Legacy human-byte expectations are replaced only
where they protect retired formatting. Report-only target blockers and non-claims
now cross typed facts instead of disappearing with old C-rendered rows. Native
client transport failures retain their established exit status and exact error
identity. Completed acquisition resumes authenticate the retained receipt under
the transfer lease, preserve stale-lock refusal and never repeat a completed
transfer merely to refresh presentation.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Operator contracts | Registry, existing JSON schemas, typed C operations and retained C executable | Isolated source/artifact/profile/account/model/engineering fixtures | Equivalent machine facts, refusal and publication boundaries | Native differential controls pass; no legacy dispatcher | Exact schema values, exit identities and emitted bytes where contractual | PASS | Exercised operator contracts, not whole-model execution |
| Chat and host logs | REPLAI interaction contract and typed local events | Real PTYs at 40/80/180 columns; styled/plain/NO_COLOR/dumb; Unicode | Safe completion, resize, exact channels, cancellation/resync and terminal restoration | Qualified fixture controls pass; SIGINT/EPIPE detach leaves host alive | No alternate screen, opaque background or stale-draft mutation | PASS | Terminal/lifecycle behavior under isolated fixtures |
| Native C consumers | Installed headers, typed C client and private protocol v24 | Independent C readers and C/C++ public-record compilation | Same records, one server owner, no upward Rust dependency | Independent consumer and package controls pass | Exact layouts; 40 public records; one executable | PASS | Preserved C ABI and tested local protocol scope |
| GNU Make | Build rules, source/package manifests and Cargo lock | Incremental/relink/clean/DESTDIR/invalid-path checks | Correct ownership and complete atomic product publication | Thirteen focused host build checks pass | No arbitrary root cleanup; library build independent of Cargo; caller link overrides retained | PASS | Tested build and staging contracts, not legal release permission |

These isolated software/PTY results do not qualify model quality, GPU inference,
Metal or the YAI/Studio chain. The source-stable mapped campaign
`520ce2f224a8924c9a713d1b2058f658f5911c32b994f442e9fab93bcdf4b7a2`
returns 130 PASS, zero FAIL/SKIP/ERROR and 33 BLOCKED across 163 selected
assertions. Its start/finish delta is
`dd1d6d95de5fa4b28c99e7748319a298c6397afd367eef661a68481318839d58`;
source stability is valid. The mandatory unavailable live/reference assets
keep the aggregate non-green; isolated shell/software evidence does not replace
them. Both mapped ASan/LeakSanitizer and UBSan lanes pass, including the final
Rust-to-instrumented-C link, HTTP, real tiny CPU model and terminal lifecycle.
Rust checks pass 51 unit tests and five structural tests; the isolated worker
lifetime test executes the deliberately ignored signal test in its own process.
The official Python 2.50.0 and JavaScript 7.1.0 OpenAI consumers also pass
model/chat/Responses/SSE/tool-loop fixtures, not real producer inference.
Clean-checkout qualification caught a C benchmark fixture outside admitted test
membership; it now lives under `tests/integration/`, without weakening the
classifier. GNU Make database inspection selects the explicit introspection
target, so recursive Cargo recipes cannot run merely to inspect build inputs.
Darwin retains its native `libSystem` pthread linkage instead of forwarding a
Linux-only linker option. Tooling components are explicit CI prerequisites.

The clean `06f234b607899586a70fc2c7b3bf11c0dbcf6bd5` mapped repair run
`6e73bb9b22751a73b844eedad8225ce5b7cf2efb455b90f49b830c722cddb223`
passes all 119 selected software assertions, including both sanitizer lanes;
its start/finish tree is clean and source stability is valid. The
[hosted Linux run](https://github.com/yailabs/yvex/actions/runs/37126530947)
passes 120 hermetic assertions with valid source stability, plus real-chat
Valgrind. Its receipt is
`a481127bcf4f77639d61f6d3cebfe1448497deffffab850265e3633fc906b9f7`.
The completion-test repair at clean `0fbb4c188f5842f5758d29844f737c2fabf947fd`
passes all 115 mapped assertions under receipt
`8dfbaf37b2e47e9cbb6a07490b299c49c0d4d497e032c391d5ca2b837b884e2c`.
These clean software gates do not resolve the separate full-model asset blockers.

Cancellation before admission is exercised with a bounded producer socket send
buffer. A synchronous cancellation acknowledgement could block the only response
reader and deadlock progress; one joined cancellation worker preserves draining
and the exact typed acknowledgement/refusal boundary. The fixture verifies one
cancel dispatch, no automatic retry after lost delivery, subsequent independent
work and restored terminal state. PTY completion synchronization starts its idle
interval after observed Escape consumption, not after the caller's write. A
deliberately delayed reader falsifies the earlier sender-side timing assumption.
The hosted Darwin trace observes correct expiry of a 250 ms producer deadline
after a 344 ms reactor wait under scheduling load; the semantic PTY control
allows delivery margin rather than pretending that a 300 ms sender sleep proves
expiry. The producer timeout and fragmented-sequence policy are unchanged, and
temporary tracing is removed. This is not a deadline-latency or platform SLA.
One-shot terminal captures drain while the child runs and before releasing the
last slave, rather than treating teardown as an output-delivery boundary.
Temporary fixture roots are canonicalized before sealed benchmark operations;
Darwin's `/var` alias cannot substitute for the canonical asset-path contract.
The Rust native pipeline fixture honors the existing hosted capacity opt-in
inside its isolated children, just like the binding and tiny-vertical fixtures.
It declares 128 GiB, refuses any caller-injected capacity override, retains the
explicit low-budget refusal/recovery controls and does not establish host memory
admission. Ordinary local execution still uses actual capacity. Compilation
failures retain captured diagnostics rather than hiding stderr in an exception.
The active-source cleanup control uses an explicitly ready process that retains
the exact source argument. Shell tail-exec optimization is not evidence of an
active argument owner; cleanup must refuse while the actual owner remains live
and preserve both source and partial state.

Clean published `25710bd4df7d082a951cbb64dec9f1d74cc6e2a9`, tree
`494a56a6cefb4c424612067601d8252ac379ad12`, passes the complete
[hosted Linux/macOS run](https://github.com/yailabs/yvex/actions/runs/37132550648).
Linux passes 120 hermetic assertions under receipt
`9f2bf10f520e1ee7734102d32d0caef682616720f80db2ae50cf0d1004ea06cc`,
plus the unsuppressed real-chat Valgrind step. macOS passes 14 native assertions
under receipt
`37b57ec55957c9e72b6c9d4a1438c940f122fc4fce1eee5ebb0f92669aea5f41`,
then ownership/build and single-product packaging checks. Both receipts record
clean valid source stability. [Native platform evidence](macos-native.md#rust-product-shell-qualification-2026-10-03)
separates declared fixture capacity from actual-memory admission and the prior
C-shell foundation. The clean local `441a9542` repair campaign also passes all
117 mapped assertions in receipt
`82134e658c2bb336a0945825cf5285fb66bfe17fd2ccf0f2d487e69a77c049e2`;
the hosted-capacity fixture change passes all 115 mapped assertions in clean
`ad155496` receipt
`fb4003031323218bf28e06da9e66f227c7cfcb3819c4b7da68e4746951c33689`.
These software claims do not remove unrelated producer/release asset gates.

The final clean source `25710bd4` also passes all 123 obligations selected by
`python3 tools/qa.py run --changed 61706207e8b96909d73ebc4a7f42bfb46716ac6e`:
zero FAIL/SKIP/BLOCKED/ERROR, including quantization and runtime
ASan/LeakSanitizer/UBSan, the three GGUF numerical gates and official encoding
vectors. The invocation supplies the exact admitted artifact/binding and pinned
source plus `tokenizers` 0.20.3, rather than treating unconfigured assets as
absent capabilities. The generated source-stability receipt remains under
`build/qa/evidence/`; this shell-only selection excludes the separately
published prefill implementation and its full external producer gates.

The running public
DeepSeek producer remains the previously qualified phase-geometry executable;
interface tests do not replace it. The compiled software package retains
`distribution.legal.v1`'s UNQUALIFIED marker: Cargo's exact shipped components
still require recipient notices/source closure before customer distribution.
No REPLAI producer, sibling repository, A03 or Laya work is included.

Operator startup characterization uses 20 samples after three warmups, plain
100-column output, an isolated empty catalog and read-only live host status.
This is not a source-identical speedup or inference benchmark. The C reference
digest is `e90be38aef648c5b4b600437f36a68da248d1093cd991d49c1c1be34b399fa6c`;
Rust release is `ec5ef403627ffe4f6a92273836a1cc866c72a4822e5457d1de635cef2d28c36d`.
Their executable sizes are 29,972,592 and 46,222,104 bytes respectively, including
the retained release debug information. Raw observations are retained outside Git
at `/tmp/yvex-shell-characterization-final-144.json`.

| Operation | C median (min–max), ms | Rust median (min–max), ms | C / Rust maximum RSS, KiB |
| --- | ---: | ---: | ---: |
| `version --json` | 1.943 (1.918–2.192) | 8.020 (6.158–10.820) | 4,856 / 10,248 |
| `help` | 1.845 (0.736–2.111) | 9.853 (7.590–11.052) | 5,256 / 10,496 |
| `model list` | 1.850 (1.386–2.481) | 7.883 (6.565–8.077) | 6,216 / 11,068 |
| `host status` | 1.828 (1.492–2.289) | 9.976 (9.156–12.180) | 5,816 / 11,084 |

The Rust build has a measured few-millisecond startup and roughly 5 MiB
process-residency tradeoff; this whole-process measurement does not isolate
registry deserialization, dynamic loading or presentation cost. No startup
speedup is claimed. Driven PTY resize is observed within
0.32–0.78 ms in the focused three-width capture, not a terminal-platform SLA.
Actual DESTDIR staging installs one Rust `yvex` and preserves its UNQUALIFIED
legal receipt. The public producer's loaded digest remains
`245561033921a35b8641b3f1731a9a864bdbbb581db6ae7dc082bdc98d12738f`;
the interface binary is not substituted into that running service.

## Phase-specific prompt geometry (2026-10-01)

`V010.RUNTIME.DEEPSEEK.GB10.PREFILL.PHASE.GEOMETRY.2` separates real prompt
work from the model's speculative verification envelope. The prior six-position
cap caused repeated prompt batches even with a configured 64-token chunk. The
generic candidate admits up to 32 prompt positions without changing source
verification, expert routing, ordered numerical class, attention or transactional
publication. [Specialization](../architecture/deployment-specialization.md)
owns the admitted masks; [generation](../architecture/generation-decode.md)
owns suffix batching. This does not reopen the previous pipeline's earned gains.

The [structured observation](benchmarks/data/deepseek-gb10-prefill-phase.json)
is the sole measurement owner; its
[generated view](benchmarks/generated/deepseek-gb10-prefill-phase.md) projects
the comparable controls. Reference main is `67a7905ea9deb98b0704629a1f979634e19007fb`,
tree `8734e779c19c78cda6d2612f08062234066e7caf`. Clean reference executable is
`c2b329083d1737253f5f86a9369f55b4e97e1001923c632ec1553cf2d4a630ca`;
candidate is `245561033921a35b8641b3f1731a9a864bdbbb581db6ae7dc082bdc98d12738f`,
compiled delta `d58e6ed3cfef97c05c9dc7a58449a6dda0a769d57e305fa8b134e4da68adcaf3`.
Candidate compilation includes the separately staged C server-loader extraction
and metadata, not the uninstalled Rust shell. Each run froze its inputs; exact
compiler provenance, source variants and raw-receipt hashes remain in the record.

Both use the same admitted mixed DeepSeek/DSpark artifact/binding, GB10 UUID,
driver, context 32768 and warm single-worker synthetic requests. Two samples
per control retain exact request bytes, token counts and result hashes at
22/106/346 input and three committed output tokens. Wider prompt geometry
materially improves the medium/longer complete requests; the short HTTP samples
do not establish a stable gain. Telemetry confirms fewer actual prefill batches
and synchronization boundaries, not more concurrent sequences. Wider physical
arenas increase session setup/cleanup and peak RSS; the separate one-sample
resource controls quantify that cost. Nested device/component spans and mapped
versus RSS byte classes must not be added.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host component owners | Canonical phase masks, expert worklist and state contracts | Eight selected runtime/worklist units, stale masks and malformed populations | Separate prompt/verification admission; unchanged associations and refusal | All eight pass | Exact masks, pair associations and untouched refusal outputs | PASS | Bounded component admission, not full-model upstream conformance |
| CUDA components | CPU decoded-dot reference with existing ordered class | Wide 32-row expert populations across IQ2_XXS/Q2_K; finite and NaN controls | Finite outputs within existing publication tolerance; canaries intact | Three selected CUDA owners pass | Existing BF16 publication tolerance; no relaxed check | PASS | Exercised wide CUDA primitive correctness |
| Real producer | Exact admitted artifact/binding and reference executable | Two warm samples each of three synthetic prompt sizes | Valid generation; exact request/usage/result agreement; no invalid status | All six candidate requests succeed and agree | Exact usage and content hash; timings retained separately | PASS | These bounded producer workloads and their measured latency delta |
| Refusal and recovery | Existing OpenAI/local lifecycle contract | Generation mismatch, output over-capacity, unloaded model; disconnect during 650-token prefill | Typed refusal; no invalid result; retire temporary work; subsequent request succeeds | HTTP 409/413/404; cancellation after 32 real input positions; recovery 200 | Zero active work and physical session state after each isolated turn | PASS | Exercised fail-closed admission and cancellation cleanup |

The operator authorized replacement only after zero active requests, queue,
HTTP work, attached clients and model leases were verified. The sole prior
session was empty. Supported close/stop retired the original process; isolated
hosts exited normally and removed their sockets. Public PID 2359674 uses the
candidate digest above at the original native socket and HTTP port 8001;
health/catalog are 200. Its recreated `main` has zero position/turns and no
attached client. Readiness verification correctly accepts a newly created
`ready` session as unbound; it does not infer attachment from that state label.
No operator Case, prompt payload or indeterminate SEND is reused.

### Source-stable software and official input qualification

The isolated producer-only source snapshot passes 127 mapped tests, with one
temporary REPLAI download failure and 34 BLOCKED tests in receipt
`dd118beedf3884e5eda36d81707fe7de9341b7a9ee20f9b8c00aa0bcf8c104f6`.
Repeating only `cuda.no-nvcc` against the unchanged snapshot passes in receipt
`51adfbf81c39b7cb799e477396ab5897b2783de8eef16b300af68cbc4f1e5b9a`.
Both verify source stability. The resolved selection is **128 PASS / 34 BLOCKED**,
not one all-green aggregate receipt. Host sanitizers and the mapped numerical,
structural, protocol and unit lanes pass within their own scope. Unconfigured
assets are missing qualification inputs, not proof that those assets do not
exist or that their producers fail.

With the exact admitted artifact, binding, source revision and independent
tokenizer environment supplied, `reference.deepseek.official-encoding` passes
in source-stable receipt
`ce86fe981d3662365898e175a50ee49b4cd437aec881fa2fda8571c63c6c4580`.
The source-authored upstream encoder supplies four vectors; four native BPE
controls and one native request-prefix control also pass. Independent
`tokenizers` 0.20.3 parity covers 13 cases and three prompt cases. Full native
transcript/tool/developer/reminder projection remains unqualified; whole-model
logits were not run. This is input-contract evidence, not an upstream inference
oracle for the prefill optimization.

This is not 14K/32K latency qualification, sustained decode, a release/SLA,
official full-model numerical conformance or YAI product-chain closure. Focused
producer implementation is published in `61706207e8b96909d73ebc4a7f42bfb46716ac6e`.
The distinct legacy bootstrap `cuda.native` asset
blocker and retained broader numerical gates remain explicit; this pass does
not erase earlier pipeline numerical failures or turn them into missing assets.
Raw synthetic evidence stays outside Git at
`/home/dgmothx/lab/models/evidence/yvex-prefill-phase-20261001.2qPmxM`.
A03 stays READY; YAI, SDK and Studio remain untouched.

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

## Atomic metadata-reader qualification (2026-10-06)

The registered acquisition-state unit reproduces a generic bounded-reader race:
with atomic replacements between two complete records, pathname sizing followed
by a separate open observes 1,787 inconsistent reads in 316,878 attempts. The
repair opens first and sizes/reads that same descriptor. Capacity, optional
absence and nonregular-file refusal remain intact; FIFO refusal cannot wait
for a writer. This is a metadata reliability repair, not inference acceleration.

The isolated ASan/LeakSanitizer/UBSan control reports zero inconsistent records
in 196,977 reads, with no reported sanitizer error. Its frozen base is
`c57d333bb3a3f63d9c0d720c8016455cdeb7da41`, tree
`b525c7379420a0530606e02e9bba5982099bd4e7`, delta
`00cd905955f765a450b6cc374c3b3b0806cb87140dcb2e2a4e88892e339bf2b9`;
receipt `59c43619e536d90b96cad83a69832c655de578e9c77c4ac94268d78a3421ce71`.
The metadata-reader and unit source bytes match the integrated competitive
candidate exactly. Replayable source, instrumented runner, receipts and the
original failing control remain under the existing external evidence root:
`/home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/software-scope-20261006.4rXu7G`.

These observations qualify concurrent atomic metadata publication at the stated
software boundary. They neither establish model quality/performance nor repair
the independent repeated-8K numerical failure. No production host was replaced
or operator request replayed for this control.

## Qualification operator consumer (2026-10-07)

The Rust shell exposes registry-derived `model qualification
list|show|suite|compare|run`. Published records, comparison rules and immutable
workloads share the [structured qualification authority](benchmarks/generated/qualification-index.md).
Inspection does not invoke Python or load weights. Native runs authenticate the
local binding against the loaded producer, preserve its selected configuration,
and emit protected local receipts; they cannot promote local observations into
published qualification. Decimal receipt statistics retain exact F64 round-trip
semantics instead of weakening the comparison oracle with a tolerance.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Rust operator contracts | Canonical registry, qualification rules and generated records | Complete shell campaign, 91 qualification acceptance/refusal controls | One grammar; compatible records only; missing provenance refuses comparison | 81 unit and five structural controls pass; mapped shell receipt `5fb101bec020aea1d0d2c9b5737ec4049970c4428a1ccfdb2f6cb1ca9d8335c9` | Exact identity/statistics; fmt and clippy without suppressed warnings | PASS | Software projection, not model/reference qualification |
| Native observer | C-owned protocol client and independently authored protocol fixture | Started/fragment/terminal events, repeated turns, malformed invocation | Correlated timing and bounded owned-session cleanup | Receipt `b4e6a21a5d826380bbf44d3ff05e130abcc7c3c458b8774220e2502ca4294bfd` | No private wire reimplementation | PASS | Native measurement adapter/lifecycle |
| Cancellation and terminal interaction | Correlated terminal settlement and REPLAI contract | Six cancellation/partial-delivery fences; PTYs at 40/80/180 columns | ACK alone cannot authorize cleanup; stale or unresolved work is journaled | Isolated cancellation and shell PTY controls pass | Plain/styled, Unicode, resize, reconnect and restoration | PASS | Tested refusal/cancellation and terminal behavior |
| Protocol/server regressions | Existing typed local protocol and server owners | Mapped unit controls | No protocol/version or server behavior change | Receipts `0e982c700b4de59d24bf2c16e5e307fd6a5b9c9a134f42b019efb64aac1ee82a` and `a15df738e70d6e9d81201a1db730e484ffe84a1b527eb6c3aad5c0d51bfd166d` | Exact existing contracts | PASS | Exercised protocol/server compatibility |

A manifest-authored short conversational case also completes through this Rust
command against the unchanged installed DeepSeek producer, and retires its
benchmark session. Its single local receipt remains under
`/home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL/qualified-cli-native-smoke-v26`.
Unknown producer/hardware provenance remains explicit; this is not a published
performance sample or independent model oracle. The installed producer retains
its speculative strategy, context and chunk; no operator Case content is used.

Separately retained candidate observations have their own generated
[target-only](benchmarks/generated/qualification-deepseek-device-ingress-coding-23.md)
and [speculative](benchmarks/generated/qualification-deepseek-device-ingress-speculative-coding-25.md)
target details. Those frozen candidate receipts do not describe the installed
producer or qualify an unexecuted configuration. Numerical, representation and
hardware gates remain independent; competitive execution/residency is still
IN PROGRESS.

## Competitive computational integration (2026-10-07)

The accepted computational integration belongs to
`V010.RUNTIME.DEEPSEEK.GB10.COMPETITIVE.EXECUTION.RESIDENCY.4`, not a new Task
or a complete throughput exit. It combines the already measured generic
certified decoded-dot realization, bounded prepared layouts, phase-specific
row/checkpoint/workspace geometry, structural CUDA-graph reuse, checked program
completion and device attention ingress. Artifact, binding, routing populations,
source-authored reasoning and ordered F64/F32/BF16 publication obligations are
unchanged. This is not a physical-variant or precision promotion.

The measured integration snapshot is `e771219f919bdb8d91a7a47702d5e252edf7fa13`,
base tree `fc34ea9ab1e2121ae2911296ecd857058ce77202`, frozen delta
`c4031f5a1b5fe42ccc9803a91a23ad608c27bff82dc1d5b310f78197d9dcadf0`.
Executable SHA-256 is
`9c42bc28f38993045bec4c5466e6c8e39c2581229e8d8a726d2e993278026377`;
native library is
`2615a9ebbbbc4b64507d048c82d8827acf096cd2b2a063aec392edbed6fd7ee1`.
The version projection now exposes the exact canonical QA source-delta
identity; Make consumes that same owner instead of encoding dirty files with
another hash. An unbound earlier build was refused before model execution and
its raw failure retained. A dirty source receipt is not described as a clean
commit merely because accepted files are subsequently published.

The [integrated product-native coding receipt](benchmarks/generated/qualification-deepseek-integrated-native-coding-29.md)
uses the same authored coding case, fresh sessions, greedy sampling, 256 output
bound, context 32768, chunk 64 and DSpark as the
[installed baseline](benchmarks/generated/qualification-deepseek-installed-native-coding-21.md).
Four sequential samples retain the initial sample separately and three-sample
warm median/MAD. Every 256-token output has the same SHA-256
`ccc7ed7525af5370a1d13dd86253f051d5d8cf974cc9deb249f588b45486fe9d`;
the proposal/acceptance population is also unchanged. Source/build/backend
implementation differ. The improvement is a complete-request observation, not
an isolated-kernel attribution or a claim for every prompt. Unknown typed
hardware/kernel dimensions stay explicit; the strict all-dimension comparator
still refuses to promote these records into a fully qualified ranking. External
device witnesses do not silently fill missing producer facts.

The [repeated 8K control](benchmarks/generated/qualification-deepseek-integrated-prefill-repeat-27.md)
is a separate frozen candidate with target-only, context 32768, chunk 512 and
8192 newly executed positions. All three independent fresh requests succeed
and produce the same bounded 16-token continuation. The earlier failure after
512 positions does not reproduce in this control; no historical root cause is
claimed. A 16-token tail is not sustained-decode evidence. Timed requests have
no profiler or periodic `smaps` walk; boundary residency and sampled process
witnesses remain diagnostics, not proof of uninterrupted exclusive hardware.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Decoded projections | Independent host ordered F64 and literal CUDA realization | F32/BF16, cancellation, rounding ties, subnormal/exceptional inputs, partial populations and prepared MXFP4 | Identical admitted F32/BF16 publication or typed refusal | Component gate `2972095` and encoded-row gate `3f905e` pass | Zero bit differences in exact publication controls; other classes keep their own tolerances | PASS | Certified numerical realization at exercised component geometries, not upstream logits |
| Program/graph/state | Typed scope, unchanged predecessor failure and independent compiled-program controls | Graph structural reuse, changed emission/history shape, workspace shortage, cancellation, stale schemas and source populations | No stale graph, forgotten status, partial publication or leaked owner | Program `495fda`, graph `b8ab8`, binding `68e181`, sequence `92d640` pass | Exact state/publication and zero owned allocation deltas where asserted | PASS | Generic CUDA lifecycle; other family-shaped fixtures retain their limited scope |
| Current computational consumers | Canonical memory/planner/prefill/decode/logits/speculation and session contracts | Seventeen mapped CPU/software controls plus tiny vertical and native observer | Correct geometry, refusal, independent state and recovery | All 17 PASS on frozen integration snapshot; tiny vertical receipt `e434b14c97021e47044e6ec95e1b553365dabfc95fab83075cac966e715685d8` | Existing per-owner assertions; no fabricated model-quality oracle | PASS | Exercised common consumers and CPU/Metal-refusal regressions |
| Real admitted model | Canonical `live.deepseek.generation`; component/manual composition and target semantic reference | Exact current mixed artifact/binding; CPU and CUDA, target-only/DSpark, deterministic/stochastic replay, acceptance and reasoning-mode controls | Equivalent admitted continuation, fail-closed mutation/capacity, cancellation and cleanup | Receipt `26f1884c3ac8755593676f2ff3385bd1f58184b4c76064f24fabfe2b0d2bcb8a`: PASS, zero FAIL/SKIP/BLOCKED/ERROR | Exact replay/target equivalence under the gate; not cross-precision upstream equivalence | PASS | Bounded full-model numerical/lifecycle execution, not quality or release qualification |
| Device memory safety | CUDA Compute Sanitizer memcheck, unsuppressed | Changed decoded-dot, program and graph controls | No invalid device memory access or leaked allocation | `integrated-memcheck-v32.log`: zero errors, zero bytes leaked | Three selected component owners; full qtype diagnostic is not substituted | PASS | Device memory safety at exercised paths |
| Official family vectors | Pinned upstream DeepSeek conversation code and native tokenizer | Four immutable upstream encoding cases and supported prefixes | Exact encoding/parsing/token identity | Official gate receipt `a6c9a569d072f77ca885aed5a898c7050bd36a5ea352c76b79118f7b16b2ccc8` PASS | Exact tokens and source-authored grammar | PASS | Official input conformance only, not authored coding/output reference |

Raw replayable source captures, benchmark observations, lifecycle wrapper and
device logs remain outside Git under
`/home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/checked-program-window-20261006.Dxl0HL`.
The complete-model lifecycle window is `integrated-generation-lifecycle-v31`;
native coding is `integrated-native-product-coding-v29`. At that historical
capture the production binary was not replaced: supported unload/restore keeps finite generation 1, approved
listeners/grants and the unchanged DeepSeek profile, restored as generation 27.
No user/Case prompt is replayed. Current producer facts are in the
[operational handoff](data/finite-lan-20261006.json).

The 20 target-only decode and 700 uncached-prefill objectives remain unearned.
Fresh final-owner profiling, broader none/high/maximum product observations,
independent representation-quality qualification and complete competitive
decomposition remain open. The missing historical bootstrap-Q2 gate and optional
full-qtype memcheck diagnostic retain their separate status. No CUDA result
qualifies Metal, another checkpoint, A03 or release readiness.

## Clean published inference profile and installed recovery (2026-10-07)

Two sequential isolated Hosts execute the same `coding.hash-table` case through
the canonical native measurement adapter: 31 new input positions, 256 committed
output tokens, greedy/non-thinking, context 32768, prefill chunk 64, one sequence
and no prefix reuse. The computational source is clean published `a444bcdd`,
tree `508e7cfeb5887fc123e200038d073ce72dc79049`; executable SHA-256
`e1ea8e02ad223a3fffb2ecc6839ee354658bd97a6e23c7e6b0651007d4ad3b4d`.
The adapter is independently frozen at `aaa2ba3a`, tree
`32d1a2218761b5fdd864ae1cbc593dc7df8f6357`. Exact build, artifact, binding,
specialization, capacity and suite identities are retained in the separate
[target-only](benchmarks/generated/qualification-deepseek-published-target-only-profile-39.md)
and [DSpark](benchmarks/generated/qualification-deepseek-published-speculative-profile-39.md)
structured LOCAL records. Missing typed hardware/kernel facts remain unknown.

Both profiles complete with zero dropped CUPTI activity records and source,
adapter and executable closure. Server-authored phase boundaries join one exact
session/request to actual kernel/API activities, including graph nodes. The
records intentionally contain **no performance measurements**: diagnostic phase
durations are not unprofiled throughput samples. The generic native-capture
adapter now retains explicit profiling declaration and observed marker names;
performance import refuses profiled, missing or conflicting instrumentation.
It does not claim to detect an undeclared externally attached profiler.

The fresh ranking identifies a distributed execution cost, not a single
residency wait: ordered projections, routed/shared expert work, attention and mHC
remain substantial. Target-only post-first-token execution records 625,413 kernel
activities and 1,987,401 Driver calls; DSpark records 248,218 and 895,968 for its
different admitted population. Context/stream synchronization, call-scoped
storage release and graph instantiation remain material investigation owners.
API waits overlap device execution and must not be added to GPU intervals as
extra CPU work. A kernel interval union is not occupancy, Tensor Core utilization
or memory bandwidth. This profile neither proves every barrier removable nor
establishes a hardware/numerical ceiling.

Before/after Linux observations show the artifact's file-backed range resident
at those boundaries, no process backing-read increment and no CPU major-fault
increment. No `smaps` walk occurs inside the coding turn. These observations do
not prove page locking, future residency or absence of GPU faults; mmap, CUDA
addressability and actual physical residency remain distinct. No whole-range
prefetch, cache eviction or representation conversion is introduced by profiling.

Only DeepSeek is unloaded for the approved isolated window. The finite engine
stays at generation 1; HTTPS and restricted-SSH listeners, pins and grants are
not changed. Supported lifecycle restores DeepSeek at generation 3 with exact
model/artifact/binding/specialization, context/chunk and strategy unchanged. A
new `chat.short` native recovery succeeds afterwards, and typed closure shows
both engines READY with zero active/HTTP/queued work, sessions, leases, attached
clients and transient bytes. This does not renew SDK/YAI Case acceptance.

Raw profiles, hashes, replayable source and restore/recovery witnesses remain
under the existing external evidence root in `published-physical-profile-v39`
and `published-postprofile-recovery-v40`. Generated target views derive from the
structured records. The competitive Task stays IN PROGRESS: independent quality,
broader reasoning/product controls and quantitative competitive closure remain;
20 target-only decode / 700 uncached prefill are not earned or weakened.

## Installed native coding and multi-turn matrix (2026-10-07)

The installed clean `a444bcdd` Rust shell and resident Host execute the authored
`deepseek-product` corpus without changing DeepSeek generation 3, finite
generation 1, context 32768, prefill chunk 64 or DSpark. Four fresh
[Metal coding controls](benchmarks/generated/qualification-deepseek-installed-metal-none-42.md)
and four three-turn
[conversation controls](benchmarks/generated/qualification-deepseek-installed-conversation-none-42.md)
complete through native protocol 25 with greedy sampling and the manifest's
256-output bound. Each conversation sample uses its own session; later turns
reuse that session's real committed prefix. Exact input/history identities and
token-ledger identities agree across all four repetitions of each turn.
The generated details own median, range and dispersion, prompt/reused/new
positions, internal/client-visible TTFT and DSpark populations/economics.
These are LOCAL product characterizations, not upstream quality or a claim for
all prompts. All four samples, including the first workload sample, are kept.

The short greeting commits ten tokens; it is a latency control, not sustained
decode. The subsequent coding and explanation turns each commit 256 tokens.
For the reused turns the benchmark publishes prefill wall time and actual new
positions, not an uncached-prefill rate. The native acknowledgement timing
accounts for most of the server/client first-token gap in fresh sessions;
its much shorter reused-turn arrival is observed separately. That difference
does not, by itself, isolate transport cost or prove every setup allocation
removable. The coding acceptance population is lower than the separately
retained hash-table control: different prompt/acceptance shapes are not averaged
into a generic DeepSeek speed or treated as directly comparable workloads.

One new coding request in each source-authored
[high](benchmarks/generated/qualification-deepseek-installed-metal-high-42.md)
and [maximum](benchmarks/generated/qualification-deepseek-installed-metal-maximum-42.md)
mode reaches 256 committed positions without the reasoning terminator. Both
return `YVEX_ERR_FORMAT` with a reset-required partial receipt; the owned session
is closed and no successful performance receipt is emitted. Their publication
records retain the settled refusal, raw hashes and cleanup, with **no throughput
measurements**. Reasoning-to-final transition and successful reasoning rate are
NOT MEASURED. The maximum source instruction, output bound and stop grammar
were not shortened, changed or retried. These bounded failures are visible in
the generated workload/reasoning matrix, not relabeled successful thinking.

The controls exposed two Rust publication gaps: a short post-first burst could
enter the sustained-decode metric, and prefix-reused work could enter the
uncached-prefill metric. The canonical qualification owner now supplies shared
population eligibility through generated rules to both Rust and the engineering
adapter. Boundary, missing/malformed-count and mixed-population controls protect
the filter. Raw observations remain unchanged; only eligible metric rows are
published. Rust observations also retain the authored logical case identity,
so multi-turn generated population views do not reconstruct it from prose.

The source/build/executable witness and sampled accelerator-process observations
remain external under the existing evidence root in `published-product-matrix-v41`
and `published-product-remaining-v42`. The adapter is frozen at `37992920`,
tree `8110302887de1a1264f24277872f1ab4d0c631c8`; the Host remains the same
immutable installed `a444bcdd` executable. There is no profiler or periodic
`smaps` walk in timed requests. Sampled clear process lists do not prove
uninterrupted hardware exclusivity; missing typed hardware/kernel provenance
still prevents a fully qualified ranking. Closure again shows both engines
ready, no active/HTTP/queued work, sessions, model leases, attached clients or
owned transient bytes. Approved HTTPS/SSH pins, grants and listeners are unchanged.

This completes the bounded installed-product matrix at its declared workload
scope, not the competitive Task. Successful longer-bound reasoning, independent
representation quality and the measured software execution owners remain open;
20 target-only decode / 700 uncached prefill remain unearned. No YAI/SDK/Studio
code, Case prompt, indeterminate SEND, Metal or A03 work is advanced.

### Attention-layer cancellation preserves committed state (2026-10-07)

The operator's `main/r98` returned `runtime attention state is invalidated` at
the same wall-clock second as the explicitly authorized cancellation. This is
not a benchmark sample or evidence of a spontaneous failure at a particular
output length. No operator conversation is replayed.

The generic attention provider marked committed state invalid when request
cancellation refused a layer before `begin`. The session finalizer subsequently
returned `YVEX_ERR_STATE` instead of the original `YVEX_ERR_CANCELLED`. The repair
removes that invalidation, preserving the committed prefix and retaining the
primary cancellation after successful abort. Actual cleanup failures and
counter overflow still fail closed. The server's separate incomplete-turn/reset
contract is unchanged.

| Control | Authority | Expected and observed | Result | Claim |
| --- | --- | --- | --- | --- |
| Target/draft, first layer/between layers | `unit.runtime_state`; committed bytes, position and identity | Before repair: cancellation is replaced by state failure. After repair: abort retains `CANCELLED`; exact committed state survives and a new computational request commits once. | PASS after repair | Generic provider/session abort semantics, not model quality |
| Malformed cancellation/counter overflow | Same typed state owner | Malformed input refuses without mutation; real counter overflow invalidates and cannot revive. | PASS | Fail-closed misuse/accounting |
| Full-model target-only and DSpark | Typed native protocol client; owned isolated sessions | Cancel after visible content returns `CANCELLED`, not model failure; observation retains the committed position. Explicit reset followed by a distinct new request completes. | PASS | Bounded complete-model cancellation/reset lifecycle, not throughput |

The live controls use the admitted mixed artifact `b669d807`, binding `8cdb4929`,
specialization `3fb4ce20`, CUDA/one GB10, context 4096, prefill chunk 64, greedy
sampling and reasoning disabled. They run through a captured public-C host over
protocol 25, not through the installed Rust/chat executable. Their candidate
is `36c16dfb` plus source delta
`75afcf201444a6c9bba76c1853c18af6039d093c850e7de41f0ee81e8ad2c645`;
native build identity is
`fcd8bce0b0712d1b3ee3d8d37ac38f3a46b9a1fcec8ff212f992dbcb62741b85`.
Independent checkpoint/representation quality and the 20/700 performance exit
are not established by these controls.

Raw source/build captures, typed client outcomes, server events and zero-work
cleanup are retained outside Git in the existing competitive evidence root,
`followup-20261007.iiQ6tR/yvex-cancel-lifecycle-window-02`. The finite engine
remains generation 1; DeepSeek is restored with speculative execution, context
32768 and chunk 64. The installed `a444bcdd` host does not acquire the fix merely
because the candidate passes. The first disposable consumer's read-only
`session.show` incorrectly supplied a mutation fence and was refused; corrected
controls retain that negative attempt and use the admitted read request.
