<!-- docs:metadata
title: DeepSeek native output recovery
id: yvex.evaluation.provider-output-recovery
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# DeepSeek native output recovery

[Up](README.md)

Task: `V010.PROVIDER.DEEPSEEK.OUTPUT.RECOVERY.0`. Program P remains separately
IN PROGRESS; this correction owns no compiler/performance intervention.

## 0731 native output and correlated feedback — 2026-10-19

**IN PROGRESS — `V010.PROVIDER.DEEPSEEK.OUTPUT.RECOVERY.0`.** One real synthetic
buffered native-call/feedback/final chain succeeds with source-authored `high`
reasoning and explicit greedy sampling. Two `none` controls produce malformed
complete outputs, correctly refused by both YVEX and the official source decoder.
The source change improves the missing outer-block diagnostic; it does not make
invalid model output valid. The successful chain uses the already installed
producer, not that diagnostic change. Program P performance and YAI Case Work
remain separate, unearned gates.

### Target and causal boundary

Source/tree/build/executable, checkpoint, artifact, binding, specialization,
backend, context and strategy remain the exact [installed 0731 configuration](#updated-producer-and-0731-installed-2026-10-18):
clean producer `3aa7e69c`, executable SHA-256 `bb3ae5149483429fe1122f92fddceea89ed93ba28480174d463af7f2450223e8`,
0731 revision `7872f01b1d1fe23eabc4c98b48bffcef5a386062`, artifact `25d29155…`,
binding `091f933e…`, one GB10, CUDA, DSpark, context 16,384, chunk 512.
All requests use seed 0, one offered synthetic function, no parallel calls,
maximum 256 output tokens and a 300-second request timeout. No Case data,
application tool execution, SEND or historical replay occurs.

| Fresh request | Changed request axis | Input / committed completion | Actual completion and result |
| --- | --- | --- | --- |
| 1 | Original synthetic request; `none`, omitted temperature resolves to 1 | 330 / 96, EOS | Two complete consecutive DSML blocks; HTTP 400; source decoder also refuses |
| 2 | Same prompt, explicit temperature 0 | 330 / 60, EOS | One complete call plus 47 trailing prose bytes; HTTP 400; source decoder also refuses |
| 3 | Greedy retained; source-authored `high` | 409 / 87, including 38 reasoning tokens, EOS | One exact offered call, HTTP 200, `finish_reason=tool_calls`; source decoder accepts |
| 4, separately authorized | Actual assistant call/history plus correlated synthetic tool result; `high`, greedy, `tool_choice=none` | 511 / 21, including 17 reasoning tokens | HTTP 200, exact final `yai-contract`, `finish_reason=stop` |

These are single functional observations, not throughput/reliability benchmarks.
Requests 1–3 use read-only debugger observation; their timing is instrumented.
Request 4 uses the restored systemd producer without the debugger. No extra
request is inferred from a successful preflight. The original three-request
authorization and the later single-feedback authorization are both exhausted.

The complete raw outputs are retained privately. In requests 1/2 the first block
ends at byte 195; the remaining 197/47 bytes violate the official decoder's
`Unexpected content after tool calls` assertion. Extracting the first block is
an offline diagnostic control, never an accepted replacement response. The
original Core completion is still unavailable: request 1 reproduces its exact
body digest, 96-token EOS outcome and 142-byte error response digest, but does
not recover or prove the original completion bytes.

For requests 2/3, the captured committed-token ledger has contiguous positions
and fragment offsets. Independent decoding reconstructs the exact parser input;
there is no fragment/HTTP duplication in these controls. Actual prompt bytes
match the official renderer using the typed definition's canonical JSON member
order (`name`, `description`, `parameters`). Incoming JSON member order is not
preserved as a semantic requirement. No instruction, delimiter or tool argument
is repaired. This bounds the observed defect to generated output under the
tested `none` policy; it does not prove universal upstream-checkpoint behavior
or independent conformance of this quantized representation.

### Source correction and evidence

`src/tokenizer/provider.c` now reports outer predicates such as `empty_block`,
`block_close`, `invoke_separator` and `trailing_content`, with a block-relative
byte offset. Existing invocation predicates remain separate. Error class,
accepted grammar, argument validation, transactional publication and the public
compatibility version are unchanged. There is no truncation, duplicate-call
removal, EOS synthesis, sampling-default change or fallback. No new ABI, source
owner, structural ADR or consumer implementation is required.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Official completion grammar | Pinned 0731 `encoding_dsv4.py`, SHA-256 `abc0d261…` | Three privately captured completions; runtime-suppressed EOS explicitly supplied to the source decoder | Refuse malformed tails, accept complete source output | Two refusals, one accepted call | Exact grammar, no delimiter repair | PASS | Fresh outputs, not recovered historical r32/Core bytes |
| Prompt and committed output | Official renderer and independent `tokenizers` 0.22.2 | Requests 2/3, exact token IDs, fragments and prompt captures | Source prompt agreement and exact committed bytes | Both prompts match; both token decodes match parser input | Byte equality; canonical tool member order stated above | PASS | No assembly/transport duplication for these controls; not full logits conformance |
| Tool arguments / feedback | `jsonschema` 4.26.0, Draft 2020-12; exact call correlation | Actual high-policy function arguments, extra-member/null negatives, returned synthetic observation | Admit exact schema, refuse negatives, preserve call ID and final value | 1 positive, 2 negatives; exact correlated final | Exact object and text | PASS | One synthetic tool/feedback/final chain, no actual tool effect |
| Native outer grammar | Registered `unit.runtime_tokenizer` | DSpark/0731 × none/high/maximum × parallel false/true; six malformed forms plus valid control | Same refusal behavior with precise private-content-free predicate | 72 negative and 12 positive new controls | Exact status and empty publication on error | PASS | Parser diagnostic boundary; no model-quality claim |
| Public buffered/SSE refusal | Registered `integration.openai` | Repeated closed blocks through actual parser/protocol/adapters; none/high/maximum | Buffered error or terminal SSE error; no tool or successful finish | Six new negative cases plus existing text/tool/feedback/Unicode controls | Exact framing and diagnostic class | PASS | Fixture-level public adapter behavior; live SSE remains unqualified |

Before the diagnostic change, the new native regression fails against the
generic outer error (`d384b271…`); after it, the focused tokenizer and HTTP runs
pass (`a8c601f5…`, `22bd0687…`). A first harness startup fails before model load
or inference because the debugger initially resolves symbols in the wrong
language; selecting C symbol lookup is qualified on the exact executable before
retrying startup. The service is restored after that failure. No failed attempt
is silently counted as a successful model sample.

The registered changed-owner run `8a49f9e3…` covers 137 tests on frozen base
`dc87febe594b4ef1960bfff92c6de27d2be108e5`, source delta
`cb38c8782202d67bc1e2d045505d84e5514d18d3fd92ae1242337f3a99a585d6`,
with observed source stability and QA build `b8cda95b…`: **133 PASS, 0 FAIL,
4 BLOCKED, 0 ERROR**. Runtime and quantization sanitizer lanes pass, including
the affected native parser and HTTP fixtures under Address/Leak and Undefined
Behavior sanitizers. Building the canonical default Rust SDK example resolves
the management-runtime and SSH-session prerequisites; their separate stable
runs pass (`90b875db…`, `ba86a19b…`). The resulting obligation coverage is
**135 PASS / 2 BLOCKED**, not a wholly green local matrix. The two remaining
native-credential SDK controls require the system D-Bus development package
(`dbus-1.pc`) absent on this host; no system package or SDK source is changed
to conceal that environmental boundary. Final canonical-document checks run
after this documentation-only reconciliation; no implementation/test bytes
change after the full software run.

### Runtime and consumer handoff

The authorized diagnostic lifecycle preserves installed binary bytes, profile,
artifact/binding, 16K context, memory reserve, management, listener addresses,
pins and grants. Post-retirement capacity still requires 124,151,859,752 bytes,
including 16,332,895,744 bytes reserve; every diagnostic admission passes its
fresh available-memory observation. No cache eviction or model-sized duplicate
is introduced. Debugger Hosts are retired gracefully and the original systemd
producer is independently restored each time. Final Host PID is 1259920,
generation 1 in that **new Host lifetime**; management PID 2458152 is unchanged.
After feedback, work/sessions/clients/leases are zero. Exon independently observes
healthy service and the exact nonempty 0731 catalog at its existing port 18001.

For a separately authorized YAI consumer qualification, use public model ID
`deepseek-v4-flash-0731`, `reasoning_effort=high`, `temperature=0`, `seed=0`,
`max_tokens=256`, `parallel_tool_calls=false`; reconcile current identities and
run full preflight first. The successful feedback carried the actual assistant
message including its separate `reasoning_content` and exact call ID. A consumer
that drops that history must qualify its changed configuration; do not infer
equivalence. No Core parser relaxation is indicated. These producer observations
do not authorize another request, mutate a Case, qualify real SSE or establish
repeated reliability. The existing installed producer already supports the
observed successful path; the diagnostic-only source change is not a prerequisite
for testing it.

Private raw evidence and executable observers:
`/home/dgmothx/lab/models/evidence/0731-tool-repair-20261019.vCbAZWaU`.
Its redacted `analysis.json` has SHA-256
`796baec7abaf94f8e05dce68e62be7fad4e86e94fbe8ce0d262562a402e602ce`;
`schema-validation.json`, `feedback/result.json`, `feedback/after.json` and
`HANDOFF.md` retain exact checks and consumer prerequisites. Raw prompts,
reasoning, responses and token records stay private, outside Git. Source
publication, installed executable identity, this bounded synthetic chain and
real YAI Work remain four distinct boundaries.

`progression_decision: proceed` — the bounded producer-to-consumer handoff.
`downstream_safe: true` — read-only reconciliation and preparation of a separately
authorized consumer probe only; not real Case Work, general `none` acceptance,
checkpoint quality or Program P completion.

## Updated producer and 0731 installed — 2026-10-18

After explicit operator authorization, the clean `3aa7e69c` producer replaces
the historical f12 Host and admits the retained **DeepSeek-V4-Flash-0731
MXFP4/Q2** representation at 16K. This closes the installation/availability
gate, not numerical fault attribution, real native-function acceptance, model
quality or Program P performance. No inference, YAI SEND or historical replay
is dispatched by this rollout.

| Identity / configuration | Verified value |
| --- | --- |
| Source / tree | `3aa7e69c05cbcf882a5b3da0a25a2bc5f6318564` / `7b8bdf90c0753b27e321b62adb0d3278fffd7c7c`; clean build |
| Native build | `c4ab0af4ba422459c9faef1c41f5d86596a375d66b575627bb52d464ee7c8ab5` |
| Running executable SHA-256 | `bb3ae5149483429fe1122f92fddceea89ed93ba28480174d463af7f2450223e8`; independently read through `/proc/3899407/exe` |
| Checkpoint / source revision | `deepseek-ai/DeepSeek-V4-Flash-0731` / `7872f01b1d1fe23eabc4c98b48bffcef5a386062` |
| Artifact | `25d29155aa86505ce0aa5a1c5535f9ed6e3b073d1803409a646b1f643423f041` |
| Binding | `091f933e11d8dd129e86d186f622c20f637ffe98b23d2d78fe118bd1a2199632` |
| Specialization | `789cca9bf51e7c9b15a3a73d617b4f68739dc1b6b5d35512446e8735ebf2fed7` |
| Public model ID / engine | `deepseek-v4-flash-0731`; generation 1 in the new Host lifetime, PID 3899407 |
| Workload envelope | 16,384 context; chunk 512; one physical sequence; CUDA on one GB10; target-verified speculative DSpark |
| Compatibility | Local protocol 25; `yvex.openai.compat.v3`; existing Exon loopback carrier unchanged |

The model receives a new registry alias through the canonical publisher; old
entries, publications and historical bindings are unchanged. The compact IQ2
research candidate is not substituted. Original f12 binary, original DSpark
artifact/binding and profile remain available for rollback, which must explicitly
request `--ctx 32768` because that saved profile defaults to 4K.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Source software | Registered CI and sanitizer owners | Integrated source and deterministic CPU/protocol controls | Compatible source gates pass | Local CI 136/136; sanitizer 2/2; hosted Linux/macOS jobs both successful, run 39125128481 | No failed hosted job | PASS | Software qualification only; no inherited model-quality result |
| Client compatibility | Private protocol 25 | Disposable empty Host, new/f12/management clients | Status/catalog reads and graceful restoration | Three clients pass; no model load or inference in fixture | Exact binary identities | PASS | Installed management can consume the new Host's existing local contract |
| Resource admission | Canonical runtime capacity owner | Exact binding, 16K, chunk 512, one sequence, speculative, greedy assessment | Preserve existing reserve and complete peak | Post-retirement required 124,151,859,752 bytes; available 126,609,977,344; system reserve 16,332,895,744 included | Status 0; no custom reserve | PASS | Sampled pre-residency feasibility, not reservation or physical residency |
| Installed model | Artifact/binding admission and engine manager | Exact baseline artifact and authenticated binding | Ready engine with requested context | Generation 1, context 16,384, zero work/sessions/clients/leases at handoff | Exact identity agreement | PASS | Model opened; not executed inference |
| Consumer reachability | Independent Exon HTTP/TLS reads | Existing port 18001 carrier and pinned management identity | Ready health and exact nonempty catalog | HTTP 200; exact model/artifact/binding/generation; management PID 2458152 unchanged | Native/public context agreement | PASS | Consumer can reconcile identity and preflight its actual request |
| Real model / YAI Work | Independent live result required | Not dispatched | No synthetic success | Unexecuted | No throughput, call, effect or feedback result | UNQUALIFIED | None |

The earlier read-only 16K assessment refused the then-observed memory extent;
after graceful retirement of the old Host it passed with the same reserve.
No cache eviction, model-sized copy or quantization change was used to make it
pass. Model load took 108.010 seconds. Observed unavailability from graceful
Host stop through the exact new catalog on Exon was 114.344 seconds; rollback
was not required. These lifecycle durations are not inference benchmarks.
Available-memory observations, mapped bytes and actual physical residency must
not be summed or substituted for one another. The actual Core request still
requires full preflight, including tool/template and output budget; the new
runtime capacity is 16K, not the previous service's 32K.

Private evidence root: `yvex-0731-rollout-20261018.NES0CF1o` under the existing
model-evidence directory. `after.json` SHA-256 is
`62b1ff0fe6475b3dfbc345e5f08ee80699ce2f18cdc15555814ffd86a7084d1c`;
`post-retirement-capacity.json` SHA-256 is
`194a18e1334f76556ff275cbb65df1be43b7bfc85fcf4bf6e7b01b9202afd11f`.
The installation, original state, registry addition, interop controls, load
logs and bounded rollout/rollback procedure are retained alongside them.
The exact consumer handoff, SHA-256
`84b9b4f58c27a48836777e63e0bcb93ee6d1df4933979378c8045c9e3765b8f2`,
is copied to Exon with an independently matching digest. No Core source,
Case binding, Skill, permission, submission or effect was changed by YVEX.

`progression_decision: proceed`; `downstream_safe: true` for identity
reconciliation and actual-request preflight. Real governed Work acceptance,
historical numerical causality and the competitive 20/700 exits remain open.

## 0731 alternative and integrated source boundary — 2026-10-18

The operator separately authorizes an isolated alternative-checkpoint check.
The first two attempts use the retained diagnostic executable `997b9cea…`,
artifact `25d29155…` and binding `091f933e…`, with 16K context and chunk 512.
Neither attempt establishes a successful 0731 provider result:

| Attempt | Observed result | Claim boundary |
| --- | --- | --- |
| First harness | Model loads and preflight admits 8,055 input positions plus the requested output; exclusive evidence-file creation then refuses a duplicate path before inference | Harness defect, zero dispatched inference requests |
| Corrected harness | One counterfactual request is dispatched; the external conservative reserve guard cancels it before any completed prefill or generated token | Resource-guard interruption, not native capacity refusal, numerical reproduction or OOM |
| Restoration | Original DSpark returns at generation 7 after the first attempt and generation 8 after the second, both 32K; installed Host and management remain unchanged | Independent health/catalog observation, not a new model-quality result |

The second guard subtracts the full artifact extent from observed available
memory before preserving the system reserve. That conservative test is not a
measurement of physical residency or a complete runtime resource model. Its
failing memory snapshot was not retained; no exact deficit is claimed. The
last passing sample and cancellation records remain available. Any subsequent
experiment must reconcile authoritative capacity and non-overlapping memory
accounting rather than silently relaxing this guard.

Private evidence root: `0731-provider-alternative-20261018.Gdi1Rvi1` under the
existing model-evidence directory. Receipt SHA-256 values are
`d7091eef463c9ee67cee10321602c73336dfe983b4c32998e24898d1fbd8e13b`
and `42428f29680721bb0df6bbc209f0ac4596297ef8db64a42b711a2aa612c8c68d`.
No YAI SEND, historical Work replay, tool execution or installed rollout occurs.

The operator subsequently requires the updated YVEX producer, not the
historical diagnostic binary, for the new checkpoint. Mainline integration
therefore precedes a new immutable candidate build. The retained compact
artifact `a54c74cd…` and binding `c730fa87…` are a different physical/numerical
realization; the old diagnostic reader refuses that binding. A newer compatible
reader is necessary but does not itself establish safe admission, equivalent
arithmetic, model quality or tool-call success. Program P's retained performance
observations cannot be transferred to a new executable without measurement.

## Original-binary control and call-cardinality defect — 2026-10-18

One separately authorized exact-wire control used the **original installed
executable**, SHA-256 `a90afc6e1c5a601c8c984e1f3a43f70ea4313b06babc9136a83db5ec29e4356d`,
clean source `f12d8818dc0b0dcf45faf8e1d9df0d2a0f3744e7`, tree `d37e75c552c834a6bce46f2bea5b982b3e436ada`,
build `19bb0fa5caf2614b2e981f44031c1701292b11eb81c15cee9c453664e90b980a`.
It ran in a disposable process, not the installed Host, with the unchanged
26,803-byte wire `dee234e8…`, original DSpark artifact `b669d807…`, binding
`8cdb4929…`, specialization `3fb4ce20…`, context 32K, chunk 512 and speculative
strategy. Preflight independently confirmed 8,055 input positions.

| Observation | Result | Claim boundary |
| --- | --- | --- |
| Full prefill | All 8,055 positions / 16 chunks; 94.4679407 s | Original executable also passes the historical failure position in a fresh process |
| Completion | 311 final tokens, HTTP 200, `tool_calls`; native 128.640393 s, client 129.804538155 s | Completed computation; not proof of historical numerical repair or sustained performance |
| Native arguments | Two offered functions; both argument objects pass independent Draft 2020-12 validation; null controls refuse | Arguments are valid, but the response violates the requested call count |
| Request restriction | `parallel_tool_calls=false`, observed two calls | Confirmed producer admission defect, distinct from the historical CUDA failure |
| Resource guard | Minimum conservative remaining margin 2,729,401,888 bytes | Sampled safety only; no physical-residency claim |
| Restore | Original generation 5→6 / 32K; same Host/management PIDs and executable hashes; Exon catalog verified | Original request count 3→3; model-unavailability upper interval 145.009065132 s |

The response is 1,593 bytes, `application/json`, SHA-256
`d082de178d1ea3beb6e0f715bca27136f9531d478d49433308b9fae8110b5d32`.
Private content, function names and arguments are not published. Exact evidence
is retained under `dspark-exact-wire-followup-20261018.7HGjogYA/original-control`
in the private model-evidence root; receipt
`daa05b8c0b25c15901868b3beff7909771b807003b43c9184a69f49884526bea`.
No YAI Work, tool execution, installed rollout or automatic retry occurred.

This removes the diagnostic modification as a necessary explanation for the
successful fresh-process prefill. It does **not** reproduce the historical
process, rule out timing/buffer/state defects, or identify the original failed
predicate. Historical numerical causality remains **unknown**. A separate,
authorized synthetic `cuda.moe_rows` run under Compute Sanitizer 2025.3.1
`initcheck` completed in 12.949876 s with zero reported errors. `synccheck`
completed in 4.322111 s with zero reported errors; `racecheck` completed in
15.161485 s with zero reported hazards, errors or warnings. Each run used the
same frozen diagnostic source and test binary, held the accelerator locks,
checked idle ownership and the unchanged reserve, and preserved resident
generation 6. Each log contains 235 encoded configurations, plus routing,
combination and quantizer controls; this is the isolated release fixture
population, not the earlier 440-configuration shared-tree population. These
checks do not reproduce whole-model state or prove absence of faults outside
their synthetic scope. Test executable SHA-256:
`151b2a5abb986a565f7c645459cb8ee039ba92143f543955f73eca940becc5be`.

A bounded independent GGUF-reader audit examines all 1,899,386,328 encoded
bytes belonging to layer 11 in the original artifact: no nonfinite F32/BF16
values, quantization scales or MXFP4 exponent encodings were found. Reader
revision `2f2d44052b7d15c9c4dd6610f6e14a5f7b2d5f3f`, NumPy 2.3.5; receipt
`59be51840da781c96cf484b0024abbbf6ccba9de3d227d3cb21fd068c6e9b701`.
This does not test activation values, arithmetic overflow, earlier layers or
historical buffer contents. It is not a model-quality certificate.

The retained journal scoped to original Host PID 1937768 contains the failure
in its first generation session, `oa-000000000001/r2`, after twelve completed
512-position chunks. Thus a preceding successful conversation in that process
is not an established prerequisite. Reuse within the failing request and
intermittent full-path faults remain unresolved. The privacy-bounded event
projection has digest
`b414f75ba4888d68af6f94bb19ecb8f084b9c1f046ee73ebe06d3a6055c0ef16`.

The confirmed output defect has a deterministic non-private witness: two
complete source-authored calls were accepted even though the sealed request's
`parallel_calls` bit was false. The parser previously bounded total calls but
never consulted this bit. `parse_calls` now rejects that policy violation
transactionally, releases all parsed calls, and publishes no result. It does
not alter source grammar, JSON checks, numerics, sampling or model prompts;
multiple calls remain valid when explicitly enabled. The correction does not
make a disobedient model output valid or explain the earlier prefill failure.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Before/after tokenizer | Sealed request and existing public contract | Public two-call DSML fixture | Refuse when false; retain both when true | Before FAIL, after PASS | Exact count, atomic empty refused result | PASS | Deterministic request-policy defect and correction |
| Cross-family | Same request constraint, admitted Qwen grammar | Two XML calls | False refuses; true admits | PASS | Exact call count | PASS | Constraint is not a DeepSeek-name policy |
| Chat buffered/SSE | Real parser → local protocol → HTTP | none/high/max × false/true × buffered/SSE | No call publication or successful finish on refusal | 12 new cases PASS | Exact protocol posture | PASS | Source-output and Chat adapter boundary |
| Responses buffered/SSE | Same parser and Responses adapter | Same 12 combinations | Error, never completed/incomplete on refusal | 12 new cases PASS | Exact protocol posture | PASS | Responses boundary |

Existing source-completion/feedback controls total 60 passing cases; the
UTF-8/Responses suite totals 64. Source-stable registered qualification on
`b70bb2cf` plus the retained shared-tree delta gives fast 104 PASS
(`df783e9d…`), ASan/LeakSanitizer/UBSan runtime PASS (`d2d4403b…`, 371.877 s),
architecture PASS (`e3b13a5a…`) and documentation PASS (`3225a2e7…`). Pinned
Python 2.50.0 and Node 7.1.0 OpenAI SDK controls pass (`9e0e79bc…`); the first
SDK attempt was BLOCKED because Node/npm were absent from PATH
(`7ec1890a…`), then reran with the existing retained Node toolchain. No toolchain
or installed service was replaced. These software results include concurrent
Program P source and do not establish a clean release binary. Final evidence-
only documentation updates are rechecked before publication. Installation and
real YAI acceptance remain separate gates. No new full-model request is
authorized by publishing this evidence.

## Exact-wire diagnostic completes with a native call — 2026-10-18

After separate authorization for one additional request, the same retained
26,803-byte wire (`dee234e8…`) completed on the unchanged diagnostic executable
`997b9cea…`, build `298bf58d…`, base `f12d8818` plus delta `d3fade9e…`.
The original DSpark artifact `b669d807…`, binding `8cdb4929…`, specialization
`3fb4ce20…`, 32K context, 512-position chunks and speculative execution were
retained. There was no YAI SEND, Work replay, tool execution or installed rollout.

The experimental guard now records failing snapshots and traceback, and admits
only journal-confirmed, successful health/catalog GETs with idle execution
counters. Non-discovery operations, incomplete observations, foreign compute
ownership and resource violations still refuse. Four positive and seven
negative CPU controls pass. Two completed discovery reads were observed during
the successful run; original model requests remained 3→3. No numerical or
runtime admission predicate was changed.

| Boundary | Observation | Earned scope |
| --- | --- | --- |
| Input and prefill | 8,055 newly executed input positions; all 16 chunks complete in 96.5341173 s | Exact-wire diagnostic passes the historical 6,144-position failure boundary on this binary/process |
| Generation and buffered delivery | HTTP 200; 140 final tokens; `finish_reason=tool_calls`; native turn 112.752229 s, client 113.871974457 s | One completed real buffered generation, not a sustained benchmark or SSE qualification |
| Native call | One function from the offered set; JSON arguments conform to its offered schema | Independent `jsonschema` 4.10.3 / Draft 2020-12 validation; invalid-null control rejected; no external schema references or tool effects |
| Resources | 55 samples; minimum available 115,137,867,776 bytes; conservative reserve margin at least 3,754,761,760 bytes | Sampled safety, not a physical-residency or peak-allocation certificate |
| Restoration | Original DSpark generation 4→5 at 32K, verified through Exon; original Host/management PIDs and executable hashes unchanged | Supported lifecycle recovery; original-model unavailability upper interval 126.939122388 s |

The 1,170-byte response is privately retained with SHA-256
`15c934265337203d962de50ca032869eb0ad8292357442951127edaf204c6f09`.
Neither its content nor its function/argument payload is published. Prompt
identity is `face4042db838ba388ee3614b756a75cd168fea5e150e15c38fc1fea3c881f72`;
tokenizer identity is `68f23b5e24f8ee3aa208a424041c306581435a0f9eaaf7da7e81d5527af0e50e`.
The original wire omits temperature and seed. The existing adapter uses its
temperature-1 default and creates a seed at admission; the sealed provider
identity also includes adapter correlation. Thus the preflight and executed
provider digests need not match despite identical wire. Historical sampling
seed and an independent historical prompt digest remain unavailable. Sampling
variation does not by itself explain a failure before any generated token.

Source comparison finds identical C/CUDA build flags and byte-identical
attention, transformer and decoded-projection CUDA images. Common, MoE and
Tensor Core images differ because of the diagnostic changes. This remains a
different executable in a fresh process; passing this run does **not** prove
that diagnostics repair the original failure, or exclude a lifetime, buffer,
timing-sensitive or input/history-dependent defect. No numerical correction
has been demonstrated. Independent checkpoint quality and real YAI acceptance
remain unearned.

Private evidence: `/home/dgmothx/lab/models/evidence/dspark-exact-wire-followup-20261018.7HGjogYA`.
Receipt SHA-256:
`e434d03524cbe48af68a0a33840f459f4e1bd71df89fa57ca2017a97cbaf5a7a`.
The next discriminating control is the original `f12d8818` executable in the
same isolated, fresh-process conditions and exact wire. Its script is prepared,
not dispatched; an additional inference requires authorization. Provider
recovery remains IN PROGRESS, with numerical causality explicitly unresolved.

## Exact-wire diagnostic interrupted by safety guard — 2026-10-18

The operator authorized one new isolated diagnostic request, bounded to 600 s,
using the privately retained original wire body, not a YAI Work replay. The
26,803-byte file was verified on Exon and after private transfer; SHA-256
`dee234e82facd2005ea534f9076378455e083f69b6a1ce994f613cf6d0c81959`.
The payload and resulting private records are not published in Git.

Execution used the final diagnostic candidate from the preceding qualification:
base `f12d8818`, delta `d3fade9e…`, build `298bf58d…`, executable SHA-256
`997b9ceab27898671cb04b66f0a06ce4b8d23dffe34762934cf879acba1e2367`.
Its implementation is published in `5f67fbe5`; that commit's hosted QA is now
completed successfully. The original DSpark artifact/binding, 32K context,
512-position chunks and speculative strategy were retained. An isolated fresh
Host admitted generation 1, allowing the complete original wire to be submitted
without rewriting its generation field. Preflight confirmed 8,055 input tokens
and the requested 2,048-token output allowance. This is a new diagnostic process
and execution history, not reconstruction of the historical process state.

| Observation | Result | Claim boundary |
| --- | --- | --- |
| Exact wire and isolated preflight | PASS: digest/size match; full requested output fits | Input/capacity identity, not successful numerical execution |
| One inference dispatch | Last reported prefill progress 2,048/8,055; zero generated; `generation.cancelled` at 26.2373169 s | Cancelled before the historical 6,144-position boundary; neither reproduction nor exoneration |
| Safety guard | `AssertionError`; exact failed predicate/snapshot and traceback not retained | Harness diagnostic gap; no numerical root-cause attribution |
| Concurrent observation | Original Host journal records `GET /v1/models` approximately 0.125 s after the guard event | Catalog activity is established; causation of the exact assertion is not |
| Sampled resources | 13 samples; minimum available memory 116,680,249,344 bytes; conservative remaining margin 5,297,143,328 bytes after mapped bytes and reserve | No sampled reserve violation; not uninterrupted residency/resource proof |
| Recovery | Original generation 3 → 4, 32K; same artifact/binding, Host/management PIDs and executable hashes; model requests 3 → 3; Exon catalog verified | Original service recovered, no installed rollout or YAI/tool execution |

The original-model unavailability upper interval was 40.250245418 s, from unload
start through independent restored-state observation. No second inference was
dispatched. The historical numerical cause and real YAI acceptance remain open.
Any next approved run must retain the actual failed guard fields and traceback,
and establish the external activity boundary before dispatch; catalog reads
must not be reported as model generation. No production arithmetic, refusal
predicate or resource reserve was changed by this experiment.

Private evidence: `/home/dgmothx/lab/models/evidence/dspark-exact-wire-20261018.HypjJaeH`.
Metadata receipt SHA-256:
`18205bea38a959ba2ff4f0cde4b3494bf7c29b4b8bb40b083977eb4ca36a0a5c`.
The provider recovery Task remains IN PROGRESS; Program P work and all
historical submissions are preserved. This record promotes no model, quality,
performance or deployment claim.

## Prefill numerical refusal — 2026-10-17

The separately operated rollout installed clean `f12d8818`, executable SHA-256
`a90afc6e1c5a601c8c984e1f3a43f70ea4313b06babc9136a83db5ec29e4356d`, build
`19bb0fa5caf2614b2e981f44031c1701292b11eb81c15cee9c453664e90b980a`.
The original DSpark artifact `b669d807…` and binding `8cdb4929…` were retained,
with a new Host incarnation and engine generation 1 at 32,768 tokens. This is
not the 0731 Program P candidate.

The new operator-authorized YAI submission
`send:tech-infra-f12d8818-20261017-1` correlated to `http-16`, session
`oa-000000000001`, request `r2`, provider identity
`fc01eb107941b7056418d25b1f6bc8a1984ab356f6b0dce94690c9e7d8240ab3`.
Retained journal metadata shows 8,055 input tokens, last successful prefill
progress 6,144, zero generated tokens, generation failure after 79.0965214 s
and HTTP 503 after 80.2062064 s. The reported message identifies deferred MoE
ordinal 11, device status 1. The tool parser was not reached; neither parser
success nor model-directed tool use is established. The Work remains
indeterminate and must not be replayed under its historical identity.

Six retained native CUDA images (MoE, Tensor Core, common kernels, attention,
transformer and decoded projections) are byte-identical between the clean
`228cf6dd` and `f12d8818` release builds. Their source owners are unchanged.
This excludes a new compiled image in those modules, not buffer/state defects,
a pre-existing input-dependent numerical failure or other execution differences.
The original status word does not encode the failing predicate. No activation
snapshot or original failing coordinate is retained; retrospective attribution
to a specific arithmetic operation is unavailable.

The local diagnostic candidate extends the existing failure word and bounded
error text, not model numerics. See the [contract](../contracts/numerical-abi.md#moe-first-refusal-diagnostics).
It adds no payload logging, GPU allocation, per-stage download or barrier.
CPU vocabulary controls and CUDA fault-injection controls are separate gates;
compiling an SM121 image does not execute or numerically qualify it. In
particular, these diagnostics do not prove or repair the historical root cause.
The candidate is not installed and is not a release-qualified correction.

Software qualification used `f12d8818` plus the stable, unpublished shared-tree
delta `1402b35267634c2821405fdd4519022bbd0fdb2ea1b247937551d61cff5cb55f`.
This includes retained Program P work and is **not** the clean installed build.

| Lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Focused CPU MoE | Closed diagnostic vocabulary and existing CPU references | 21 known/legacy/unknown codes; routing, scheduler and result-view controls | Exact names and unchanged refusals | Receipt `03c17be7…` | Exact strings; existing numerical tolerances unchanged | PASS | Host-side diagnostic projection only |
| Fast regression | Registered `fast` lane | Disposable CPU/protocol fixtures | Existing behavior retained | 104 PASS, 0 FAIL/ERROR/BLOCKED/SKIP; receipt `6e892563…` | Registered assertions | PASS | Software regression scope, not device execution |
| Structural | Registered `structural` lane | Architecture, ownership, ABI, documentation and publication controls | Existing boundaries retained | 20 PASS, 0 FAIL/ERROR/BLOCKED/SKIP; receipt `b285eff2…` | Registered assertions; nested repository suite retains its one conditional skip | PASS | Structural compatibility, not numerical conformance |
| Focused sanitizers | ASan, LeakSanitizer and UBSan; CPU reference tests | Ten runtime owners: binding, decode, generation, logits, MoE, prefill, sampling, speculation, state, transformer | No sanitizer finding | Exit 0; log SHA-256 `32c4bbdd66b57d6bf82a5bbb4a562763847b979fb0542819c2340e614ddd3942` | Halt on first finding; leak checking enabled | PASS | Selected CPU controls, not the complete registered sanitizer lane |
| Offline CUDA compilation | Canonical Make/NVCC SM121 build | MoE, Tensor Core and common kernel images; changed CUDA test translation unit | Compile without diagnostics | All compiled | No device dispatch | PASS | Build compatibility only |
| Device diagnostics / equivalence | Independent scalar references and injected exceptional operands | Registered `cuda.moe_rows`, including first-error preservation and six quantizer cases | Correct tested predicates and unchanged admitted output | PASS; receipt `068028bc…`, 144.456244 s device-test process | Existing tolerances; exact controls where contracted | PASS | Bounded synthetic coverage below, not all predicates or the failed model request |

The focused sanitizer executable SHA-256 is
`da6b8901ad8a78d5acec0227d6978f50c126758e6dc39cb01b0b9127fd787769`.
Its build and logs are retained under `/tmp/yvex-moe-diagnostics.N5Hn7ucU`;
canonical receipts are under `build/qa/evidence`. These are local evidence,
not published model qualification. The initial CUDA compile rejected a heavy
internal header's C atomic type; routing the vocabulary through the existing
lightweight execution-observation owner resolved that build failure. No model
computation or original numerical refusal was repaired by this change.

The operator subsequently authorized only `cuda.moe_rows`, with a 600-second
limit, idle-resource preflight and preservation of the resident DSpark. One
registered test ran with source delta
`3b0e73b1a5dd11546c12fe9bfb8cdd12c64e6f79fd5ff4f73a130da6fa2d3f9c`,
build `3b2d3f5197347afdfb913ff8fa41419fdaf25527f666562beaff8ab5a5988a7a`
and test executable SHA-256
`d9c8e2589b5f774f6ec397f1d19cf325d1a8558a2c80b28a249cf11dee808da9`.
The source remained unchanged throughout execution. The guarded procedure took
147.283 s; the test process took 144.456244 s. No timeout or numerical failure
occurred. The canonical receipt reports PASS=1, FAIL=0, ERROR=0, BLOCKED=0,
SKIP=0.

The log retains 440 encoded-matrix configurations: 282 successful numerical
cases and 158 expected refusals, including 22 retained legacy errors, 44
geometry refusals, 54 gate-operand and 38 down-operand refusals. Twenty-two
successful exceptional recoveries compare exactly; the maximum observed
error/tolerance ratio across the reported configurations is 0.97164885, below
the unchanged acceptance bound of 1. Routing additionally distinguishes
nonfinite score (16), invalid normalization (17) and preservation of an earlier
down error (20), in single-row and multi-row implementations. Six quantizer
controls distinguish legacy (1), normalized input (27), routed intermediate
(28), and preserve a pre-existing router error. These are synthetic values,
not private model contents. Separate injected witnesses for every other new
predicate are not established by this run; in particular, no comprehensive
buffer-race or model-state correctness claim follows.

Before dispatch, no competing compute process or accelerator test lock was
observed. The guard protected all 95,050,210,272 mapped artifact bytes from its
available-memory estimate, plus the canonical 16,332,895,744-byte system reserve.
This conservative deduction is not a measured physical-residency total. Across
140 sampled observations, the remaining margin was at least 13,374,283,296 bytes
and free memory at least 8,776,519,680 bytes. A disposable systemd scope enforced
MemoryMax=2 GiB and MemorySwapMax=0; its retained intermediate high-water reading
was 369,369,088 bytes. The final cgroup peak is unavailable after scope removal;
the cgroup reading does not measure all driver/device allocations. The first
scope setup refused unsupported `MemoryOOMGroup`; it dispatched no test, and
the supported MemoryMax/MemorySwapMax setup then ran successfully.

The original Host PID 1937768, executable SHA, engine generation 1, artifact,
binding and 32K context were unchanged. Model requests stayed 3→3 and completed
HTTP requests 30→30. Host and management remained active with zero restarts.
After cleanup, only the original Host appeared in the CUDA process list and
the accelerator test lock was released. Sampled clear observations do not prove
an uninterrupted hardware reservation. Raw positive/negative logs, before/after
snapshots, guard script and receipt are retained at
`/home/dgmothx/lab/models/evidence/moe-first-refusal-20261017.T2luHYlv`.
Raw log SHA-256:
`cd952f5cd8f6a9af34dddfd5351585253559b0fb6bb052d0111dc146ec6fa82b`.

At that synthetic checkpoint, remaining gates included diagnostic coverage, a
coherent candidate separate from unpublished Program P changes, and a separately
authorized isolated reproduction with fresh request identity. Such an experiment
must retain the first predicate, phase geometry, exact artifact/binding/build
and resource observations without retaining private activations or replaying
the indeterminate YAI Work. This bounded CUDA-test approval authorized no model
load/unload, service restart, rollout, YAI SEND or Case mutation. None occurred.

### Attribution boundary after the synthetic control

Read-only inspection of the installed `f12d8818` source followed the row-batch
ingress, workspace initialization, graph capture/replay, result transfer and
deferred completion owners. This is a source audit, not an execution witness:

| Boundary | Implemented control inspected | What remains unproven |
| --- | --- | --- |
| MoE ingress and scratch | `moe_cuda_batch_ranges` initializes the status word; `moe_cuda_batch_inputs` copies the current ingress before graph execution | Whether the failed request supplied finite, correct ingress values |
| Graph replay | `yvex_cuda_graph_execute` updates kernel parameters and checks replay kernel count and workspace cursor | Whether the complete failed execution had a lifetime or ordering defect |
| Result publication | Device result transfer checks ownership, extents and overlap before copying to caller-owned buffers | Correctness of all preceding writers in the real request |
| Deferred refusal | `runtime_moe_rows_complete` checks pending layers in ordinal order; `transformer_moe_complete` preserves an already reported primary error | Which device predicate first rejected, and whether its input was corrupted upstream |

The synthetic test calls device kernels with constructed inputs. It does not
exercise this complete runtime ingress/graph/workspace/completion chain. Its
passing result therefore neither exonerates that chain nor identifies an
originating kernel. No demonstrated alias, missing replay update or incorrect
status lifetime was found in this inspection; absence of a source finding is
not proof of their runtime correctness.

The historical integer `1` cannot be decoded retrospectively into the new
predicate vocabulary. A fresh instrumented execution requires separate
authorization; it is not a retry of the indeterminate YAI submission. A
synthetic failure of another predicate qualifies diagnostics only, not this
request's cause.

The operator subsequently authorized the following bounded discriminator.
Its first execution is retained below; the conditional second execution was
not eligible because the first did not reproduce the refusal.

| Item | Bounded experiment |
| --- | --- |
| Source | Clean `f12d8818` plus reviewed diagnostic-only changes, excluding unpublished Program P algorithms; qualify and bind the actual executable before dispatch |
| Model | Original DSpark artifact `b669d807…`, binding `8cdb4929…`, context 32,768; no 0731 substitution |
| Population | A newly authored public fixture, tokenized to exactly 8,055 input positions, no reused prefix, chunk 512, at most one generated token; a new local request identity, never a YAI submission |
| First execution | Ordinary physical execution with the qualified predicate vocabulary; retain chunk progress, first refusal code, layer/row/pair geometry and resource observations, not private activation values |
| Conditional control | At most one additional execution, only if the first reproduces a refusal and a single-variable diagnostic control can distinguish its upstream origin; otherwise retain NOT REPRODUCED and stop this experiment |
| Deadline | At most 600 seconds per inference, at most two inference dispatches; load/restore time is separately measured, not represented as part of that inference bound |
| Resource boundary | No simultaneous second model-sized resident engine. A separately authorized temporary unload/reload is required if the isolated candidate cannot coexist under canonical admission and the unchanged reserve |
| Recovery | Preserve the installed Host binary/listeners/grants; verify recovery of the original artifact/binding and 32K profile, with a new generation if unloading was authorized |

The original input is not available to this experiment and equal token count
is not equal activation/routing history. A successful public-fixture run cannot
close the original incident. The existing `engine_scheduling` option also
changes admitted width and compatible scheduling in addition to eager graph
execution; toggling it is not an isolated graph-replay A/B. Any conditional
control must retain the same physical population and numerical realization and
record its diagnostic perturbation explicitly. No timing claim follows from
instrumented or synchronized execution.

### Isolated original-DSpark discriminator — 2026-10-17

Result: **NOT REPRODUCED**, not repaired. A genuine detached checkout at
`f12d8818dc0b0dcf45faf8e1d9df0d2a0f3744e7`, tree
`d37e75c552c834a6bce46f2bea5b982b3e436ada`, contains only reviewed first-refusal
diagnostics and their tests. Unpublished Program P algorithms were not imported.
The frozen source delta is
`e23f3599bd8795b9ec76d3b8ce8eaa996620d684bffcc5eccad17106eac9cc31`,
build `4155a3e72e4ab3867ca458a0eeaec8da9baabbb409a27bd3fba85d225348cba3`,
executable SHA-256
`b9a29b6c27222e33337872a8b784d0b457dc25a169f622bd431b8535f6c67162`.
The local protocol remains 25 and the HTTP compatibility profile remains v3.

| Boundary | Authority / input | Observed result | Claim / limit |
| --- | --- | --- | --- |
| Software | Registered fast lane on the isolated source | 104 PASS, no failures, errors, skips or blocked tests; receipt `ab68be9a…` | Software regressions, not historical causality |
| CPU sanitizers | Registered `sanitizer.runtime` on the same frozen source | PASS in 551.166 s; receipt `05945cd9…`; ASan/LeakSanitizer and UBSan include native/HTTP/PTY controls | CPU/runtime safety scope, not CUDA race or original-input qualification |
| Structure / publication | Registered structural lane and explicit documentation rerun | First run 19 PASS / 1 FAIL (`markdown` missing), receipt `3b75165b…`; documentation rerun PASS using the existing docs environment, receipt `7399d78…` | All 20 selected obligations covered; original environment failure retained |
| Device fixture | Registered `cuda.moe_rows`, original release test population plus six quantizer controls | PASS, 4.286 s; receipt `b742f640…` | Isolated-candidate diagnostics; not the earlier 440-configuration Program P test population |
| Native execution | Original artifact `b669d807…`, binding `8cdb4929…`, specialization `3fb4ce20…`, 32K context, chunk 512, DSpark, none reasoning, temperature 0 | All 8,055 uncached input positions completed in 91.8560424 s; one final token; HTTP 200 | One public-input execution, not independent checkpoint quality or a throughput benchmark |
| Product completion | Buffered Chat Completions; fresh local request, maximum one output token | `finish_reason=length`, 8,055 prompt + 1 completion token; 93.159250656 s client request duration | Valid bounded completion, not natural EOS or tool-call acceptance |
| Restoration | Original installed binary, Host and management identities, protected configuration hashes, exact artifact/binding/profile | Original DSpark restored at generation 3 and 32K; both original service PIDs retained | No rollout, service restart, YAI SEND or historical Work replay |

The fresh provider request identity is
`12815f8a8009bef564ba1494114128eb57d0a6930b66c95d52c343c18e616093`.
Its HTTP body is 744 bytes, `application/json`, SHA-256
`d315db68f268f2af11d1f93134417f57a97c6ef53f31a3bcd2b7d980be90e751`.
The fresh public text is retained locally; no private YAI contents or model
activations were captured. Equal input length, artifact and chunk geometry do
not reproduce the original activation, routing or preceding process history.
No first-failure predicate occurred, so no originating numerical operation was
identified. The permitted conditional second inference was not dispatched.

Resource observations retain 46 samples, minimum MemAvailable
115,644,211,200 bytes and minimum MemFree 980,201,472 bytes. The guard deducted
the full mapped artifact (95,050,210,272 bytes) and unchanged system reserve
(16,332,895,744 bytes), leaving at least 4,261,105,184 bytes. This conservative
margin is not a physical-residency measurement; free pages alone exclude
reclaimable file cache. Device/process observations were sampled, not proof
of uninterrupted exclusivity.

Two preliminary attempts are retained, with zero inference dispatches. The
first rejected an unsupported `--json` flag on the unload command before any
unload. The second unloaded the original engine but refused candidate admission:
113,837,204,008 bytes required versus 8,579,964,928 available under an added
104-GiB test cgroup cap. Source inspection found that clamping `system_total`
to that cap prevents the existing `device.total_memory_bytes == system_total`
shared-memory check in `capacity_hardware` from matching. Removing only this
test-added cap allowed normal admission; neither native reserve nor operator
configuration was weakened. This is a separate capacity-accounting finding,
not evidence of the historical numerical cause. The original profile was
restored at generation 2 after that refusal. The successful experiment then
temporarily unloaded generation 2 and restored generation 3. Measured
unload-start to verified-restoration intervals were approximately 7.23 s and
105.77 s respectively, not transport downtime: Host/listeners remained alive.

All retained evidence is under
`/home/dgmothx/lab/models/evidence/dspark-numeric-attribution-20261017.KJiGhsSf`:
the detached source, build/QA logs, `synthetic/`, rejected attempts and
`attempt-3/` request, resource, stage and restoration records. The installed
executable remains the original `a90afc6e…`; the diagnostic candidate is not
installed or a numerical repair.

Post-execution review found two diagnostic codes transposed during extraction
from the shared tree: `moe_combine_rows` reported weighted accumulation and
`moe_accumulate` reported shared combination. Main's original diagnostic edits
had the intended mapping. No failure predicate fired during the public-input
execution, so it supplies no positive coverage of those labels. The exact first
binary and patch are preserved as `first-execution-yvex` and
`first-execution-source.patch`; corrected diagnostic source must not inherit
their build identity. This extraction defect changes error attribution, not
arithmetic, and is unrelated to the installed producer's legacy status 1.

The new eight-case combination/accumulation fixture fails on that extracted
candidate (receipt `b498733f…`) and passes after correcting only those two
codes (receipt `595fb2a0…`, 3.994 s registered `cuda.moe_rows` run). It checks
exact finite output, nonfinite input, finite-input overflow and preservation of
a prior error, independently for both consumers; refused outputs remain
unpublished. The negative process was too short for the sampled CUDA process
list to observe it; the retained device-test assertion is the execution witness,
not the sampler's `gpu_dispatched=false`. Its 70.116 s registered duration
includes rebuilding and is not a kernel duration. Both guarded runs preserve
the original generation 3 and installed binary.

The corrected, uninstalled diagnostic candidate has source delta
`d3fade9ed9c4ff09495676e0f6ebe87279654702e4a0a17891c8f2021b49b674`,
build `298bf58d0da43a14ad8b07a97683e2e7a40ca933fe7b2bf3a8ac5aca50d3052c`
and executable SHA-256
`997b9ceab27898671cb04b66f0a06ce4b8d23dffe34762934cf879acba1e2367`.
The green test executable is
`151b2a5abb986a565f7c645459cb8ee039ba92143f543955f73eca940becc5be`.
This last source was not run through another full-model request: the successful
8,055-token observation belongs exclusively to the earlier frozen identity.
Logs and source-stable before/after resource records are retained in
`synthetic-label-red/` and `synthetic-label-green/` under the same evidence root.
The final source also passes all 104 fast-lane tests (receipt `457e5de7…`) and
the code-policy rerun (receipt `de72560d…`). Earlier complete structural and
CPU sanitizer receipts retain their earlier source identity; the final delta
changes two CUDA error labels and the CUDA regression fixture, not CPU/runtime
implementation. No hosted CI or installed rollout is inherited from local QA.

The next causal prerequisite is an authorized input-equivalent computational
witness, not another arbitrary 8K prompt. YAI must first establish whether the
exact admitted message/tool serialization or token input is still recoverable,
with its digest and rendering policy, without redispatching the historical
Work. If it is unavailable, historical causality remains unknown. A new,
separately authorized local diagnostic request may use such a witness without
executing tools or mutating the Case; it must preserve model/binding/chunk
geometry and explicitly distinguish any changed process history. No further
GPU inference or diagnostic rollout is implied by this negative result.

## Demonstrated defect

The tokenizer's canonical `append_tool_calls` renderer emits one LF for an empty
argument body before the invocation close. `parse_invoke` previously tried to
read that LF as the start of a parameter and returned `YVEX_ERR_FORMAT`.
Consequently a complete, offered function call with JSON arguments `{}` failed
the producer's own rendering/parsing roundtrip.

The authenticated DeepSeek DSpark source revision
`62af8fffb2f7030cac4de2f0169f5b8d1101b646`, `encoding/encoding_dsv4.py`
SHA-256 `bdbd57c132a1b3725042323d02b98b9d1df28e5f388f134399555d041f5055e0`,
independently produces the same bytes from its invocation template and empty
argument encoder. This is a source-serialization reference, not a claim that
the upstream completion parser or full checkpoint has been qualified.

The correction consumes one LF only when it is followed immediately by the
admitted invocation-close delimiter. It does not relax JSON validation, skip
arbitrary whitespace, accept duplicate parameters, change offered function
matching, repair missing delimiters or promote prose into operations. Existing
accepted empty-body spelling remains compatible. No public ABI, local wire,
artifact, binding or conversation-policy identity changes.

## UTF-8 transport defect

A second deterministic witness uses a valid native DSML parameter containing
4,085 ASCII characters followed by `é🌍`. Parsing succeeds. The session's
4,096-byte argument fragments can bisect `é`; the former adapter serialized
each raw fragment directly into an SSE JSON string. Buffered assembly was
valid, but Python's independent strict UTF-8 decoder rejected the SSE bytes
(`1155cd85…`). This is a public delivery defect, **not** proof of the historical
r32 format failure.

The adapter now validates complete scalar prefixes in its existing result
buffers, independently for final text, reasoning and each call ID. It retains
at most three unfinished bytes per channel, promptly emits complete prefixes
and refuses malformed or terminally incomplete UTF-8 before successful finish.
The tokenizer's scalar decoder and this boundary share a core syntax primitive;
Unicode classification and source-output recognition retain their own owners.
There is no new text store, replacement character, model allocation, public ABI
or local wire change. Non-complete prefixes that already imply an overlong,
surrogate or out-of-range scalar refuse rather than wait for more bytes.

Expanded source-stable `integration.openai` receipt `f66b6841…` passes 36 native
source/Chat cases, 52 independent UTF-8/Chat/Responses cases and three correlated
feedback continuations. Controls include nonempty arguments, actual 4-KiB cuts,
single-byte scalar splits, none/high/maximum source output, interleaved call IDs,
malformed bytes and unfinished tails in all three output channels. Interleaved
valid calls keep independent arguments; stray continuation bytes in a second
call remain refused. The existing pinned
Python/JavaScript OpenAI SDK controls also pass. Synthetic malformed-wire
fixtures deliberately bypass native completion parsing; valid-source fixtures
do not. No fixture claims model execution or model quality.

## Evidence boundary

| Lane | Authority / fixture | Expected and observed | Supported claim |
| --- | --- | --- | --- |
| Regression witness | `unit.runtime_tokenizer`; exact empty-body completion and zero-parameter schema | Before correction FAIL (`b6fa2240…`); initial corrected control PASS (`54008bf4…`) | Reproducible false format rejection, independently of GPU inference |
| Source serialization | Authenticated official template and empty argument encoder | Exact completion SHA-256 `477e41b5…` | Agreement with the source-authored empty-body bytes, not full-model quality |
| Native tokenizer | DSpark and independently compiled 0731 policies; none/high/maximum; output/call identities and correlated feedback | Complete `{}` call and exact channel/identity preservation; malformed, incomplete, unoffered and duplicate controls refuse | Bounded native parsing and feedback semantics |
| HTTP / SSE | Actual native parser, local protocol and production adapter against a disposable fixture Host | Initial 24 cases and three feedback continuations; expanded 88 cases preserve Unicode and terminal failure, with no fabricated finish | Buffered/SSE projection and terminal failure preservation, not real model execution |
| Compatibility regression | Existing protocol, provider, OpenAI and official pinned SDK fixtures | Canonical changed-owner QA and sanitizer receipts are retained below | Exact test scope only; missing or failed gates cannot be inferred PASS |

Each local receipt binds its own source-stable snapshot. The broad code gates
below use `ceeca35f` plus dirty delta `562af6a3…`; the earlier adapter receipt
has its own declared delta. The unrelated Program P candidate is retained,
not published by this repair:

| Gate | Observed | Receipt / exact limitation |
| --- | --- | --- |
| `fast` | PASS=103, FAIL=0, SKIP=0, BLOCKED=0, ERROR=0 | `00e013a3…`; includes tokenizer, provider, protocol, DeepSeek/Qwen/Mamba and tiny CPU vertical controls |
| `integration.openai` | PASS=1, no failed/skipped/blocked/error gate | `8ca9eae3…`; actual buffered/SSE adapter and pinned OpenAI Python/JavaScript clients |
| `sanitizer.runtime` | PASS=1, no failed/skipped/blocked/error gate | `b6355711…`; ASan/LSan/UBSan, native tokenizer, HTTP fixture and CPU lifecycle; no GPU model execution |
| `runtime` initial broad run | PASS=44, FAIL=4 | `83596c00…`; three SDK fixture prerequisite failures and one inherited Program P Rust formatting failure, not relabeled PASS |
| Three prerequisite repairs | PASS=3 across three separate reruns | `03978c42…`, `8b433e1f…`, `18615379…`; unchanged SDK built with `native-credentials,keyring/vendored` and an owned disposable Secret Service, not the operator vault |
| Remaining broad Rust shell gate | FAIL at `cargo fmt --check` | `src/cli/rust/ffi/execution.rs` already differs in the frozen pre-repair Program P capture; this wave neither introduced nor modifies it |

The remaining formatting failure is not a parser regression or a green global
runtime claim. Publication must stage only the owned correction/tests/docs;
published-main hosted QA remains a separately observed boundary.

### Final byte-fragment regression controls

The expanded implementation was tested at parent source `2e76c9ed…`, tree
`6fe9fd9e…`, retained dirty delta
`3aca754b777c15c9d63977654e8afe6110661be07c4cd30f5115efe6300e074d`.
Every following receipt reports source stability as valid. The final editorial
receipt additions do not change the tested C implementation. The replayable
source and observed fixture binaries are retained in `source-capture-utf8/`
under the same external evidence root. Capture authenticates observed bytes;
the QA receipts separately bind build and execution.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fast` | Canonical registered tests | CPU, tokenizer, provider, protocol and family controls | No changed-owner regression | 103 passed; `df2ca3be…` | FAIL/SKIP/BLOCKED/ERROR=0 | PASS | Exact registered software scope, not live model behavior |
| HTTP source and UTF-8 | Official serialization plus Python strict UTF-8/JSON | 36 source/Chat and 52 byte-fragment/Chat/Responses cases; three feedback continuations | Exact valid bytes/identities; malformed output never succeeds | All passed; standalone `f66b6841…`, repeated in `07f8b176…` | Exact strings, call IDs, arguments and finish classes | PASS | Buffered/SSE producer projection and refusal, not inference |
| `runtime` | Canonical lifecycle/adapter and pinned SDK tests | Disposable CPU/protocol services and owned Secret Service | Preserve lifecycle and compatibility | 47 passed, 1 failed; `07f8b176…` | Rust formatting failure in pre-existing `ffi/execution.rs` | FAIL | Passing individual controls only; no global runtime PASS |
| `structural` | Canonical ownership, ABI, architecture and project control | Shared source with protected Program P work | No introduced structural violation | 20 passed; `21c14ce3…` | FAIL/SKIP/BLOCKED/ERROR=0 at QA gate level | PASS | Existing ownership and public contract preserved |
| `sanitizer.runtime` | ASan/LSan and UBSan, halt on error | Native C, complete HTTP fixture, CPU lifecycle and PTY | No sanitizer finding or test failure | Complete rerun passed; `002d98a6…` | One canonical gate; FAIL/SKIP/BLOCKED/ERROR=0 | PASS | Instrumented software paths only, not CUDA or checkpoint quality |

The first expanded sanitizer attempt (`891b5648…`) remains a FAIL receipt: a
network read timed out while acquiring the pinned independent GGUF reader,
before its HTTP portion. It is not relabeled PASS or erased. The unchanged
source rerun completed both sanitizers and both HTTP fixture populations.
The remaining Rust formatting failure has the same pre-repair file hash
`bd34914f…`; this repair does not stage or format that Program P file.

No model/backend numerical implementation, public schema, artifact/binding,
SDK source, service configuration or YAI boundary changes. The structural
correction reuses existing tokenizer, core syntax and adapter owners; no new
subsystem or structural selection is introduced. Program P remains independent,
and this producer Task's installed/real-model/YAI gates remain open.

Raw receipts and the independent serialization script remain under
`/home/dgmothx/lab/models/evidence/deepseek-output-recovery-20261014.BdBPreBo`;
canonical QA records remain under `build/qa/evidence/` and retain source
stability, build, test identity and result. Fixture prompts and observations
are synthetic; no private operator prompts or real Case data enter this record.

## Historical causality and live prerequisites

The real YAI attempts ending after 150 and 312 generated tokens remain
indeterminate. The original r32 output and `error.message` are unavailable.
The empty-body defect reproduces the same failure class but does **not** prove
either historical request contained that structure. Other format refusals,
including genuinely malformed output or an incomplete reasoning boundary,
remain valid and are not reinterpreted as successful results.

Observed protected state: installed Host source `337e7e73`, DeepSeek generation
8, context 32K, artifact `b669d807…`, binding `8cdb4929…`. The source correction
does not change that process, executable, generation, listeners, management,
grants or runtime configuration. No real inference, GPU benchmark, model
load/unload, YAI replay or cross-repository mutation is dispatched by this wave.

A live gate requires separate operator coordination: source-stable corrected
binary, idle resource/lifecycle verification, an explicitly approved rollout
and restoration plan, then a **new** isolated probe identity. Proposed minimum
probe population: ordinary text, empty native call, nonempty native call and
correlated feedback, separately buffered/SSE (eight requests total), bounded to
512 new tokens per request. Its actual model admission, context, resource cost
and deadline must be preflighted against the observed generation; no latency
promise follows from fixture tests. Independent YAI governed Work acceptance
follows only after that real producer gate and consumer identity reconciliation.
Neither historical Work identity may be reused or automatically retried.

## Generation-31 HTTP-200 response forensics — 2026-10-16

**Classification: `INSUFFICIENT_RETAINED_EVIDENCE`.** This is a new incident
on the corrected installed producer, not a reproduction of historical r32.
The running Host executable matches `228cf6dd2ca599524d6ed5256e2faa6bf3b27665`,
SHA-256 `feab11bad6ac0e286c7ecb5804c7d1b99d9b353552f7b554b997732d504b9f3b`.
The original DSpark Engine remains generation 31 at explicit 32768 context.
Core's installed executable SHA-256 matches its recorded `ff2ff541` build;
its current repository HEAD is a separate identity.

The retained journal associates `http-1381` with Session `oa-000000000006`,
request `r17` and turn `t1` in Unix-ms interval
`1792157932560–1792158075914`. It records 299 generated tokens, native EOS,
HTTP 200 and terminal transport code zero, with no generation failure or
cancellation. Native EOS is not proof of a particular HTTP `finish_reason`,
valid native call, complete delivered JSON or accepted consumer result.
The HTTP access event's `a` field is the peer TCP port, not response bytes.

The consumer's `058c5299…` identity is the preflight provider request;
the generation journal contains `097bf701…`. The linkage uses the unique
interval, exact input population and HTTP-to-Session association, not an
unavailable producer request-wire digest. The admitted body SHA-256
`4f234095…` identifies the **request**, not the response. No exact response
body, Content-Type, byte length, digest or observed delivery mode is retained
in the audited producer journal or canonical consumer captures. The producer
frees its buffered JSON after delivery; Core's failed response never reaches
accepted ProviderResult storage. No process-memory dump, payload reconstruction,
new inference or replay is used to fill that gap.

### Consumer boundary and controlled compatibility

At exact Core source `ff2ff5416dcd5230de73ef215f9f64ff987eca07`,
`provider_execution.rs:391–405` classifies multiple `provider_response_invalid`
causes as `response_schema_invalid` / `JsonParse`; it does not retain their
specific cause. The response path additionally contains protocol normalization,
deployment fences, native function validation and capability lowering.
Consequently the original first failing predicate cannot be recovered from
the broad stage label. A JSON syntax defect, producer defect, consumer defect
or framing mismatch is not established by this observation.

The exact extracted native decoder, strict JSON parser and bounded 2020-12
schema owner were compiled without modifying their bodies, using Core's pinned
Serde/JSON/schema-library versions. This is component qualification, not a
build or execution of the whole Core controller. Its single-call limit remains
intentional; Core requests `parallel_tool_calls=false`. Neither side's contract
is relaxed.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Producer/native-consumer contract | Corrected `228cf6dd` production adapter and exact extracted `ff2ff541` native decoder/schema/strict-JSON functions | 27 synthetic text, empty/nonempty/Unicode calls and correlated-feedback responses, none/high/maximum; buffered and independently reassembled SSE | Accept valid shapes and preserve call identity, function and arguments | 27 accepted with exact field controls | Byte/structured equality; fixed dependency versions | PASS | Bounded component compatibility, not the original response or Case Work |
| Producer refusal | Actual source completion parser and production HTTP/SSE adapter | 12 incomplete/malformed source outputs | Refuse without a fabricated successful finish | Buffered error or terminal SSE error, no successful completion | Exact status/framing controls | PASS | Existing malformed-output refusal remains intact |
| Consumer refusal | Same pinned extracted decoder | 26 malformed JSON/UTF-8/trailing data, model/population/role, call/argument/schema and byte-bound controls | Preserve each specific refusal | All expected refusal codes match | Exact error-code equality | PASS | Distinct validation causes exist; the original generic label does not select one |

SSE reassembly in this experiment is independent test code; it does not qualify
Core's complete streaming router, transport or Case-specific capability lowering.
All 39 HTTP exchanges target disposable CPU/protocol fixtures on an ephemeral
loopback port, never the installed listener or GPU. Complete synthetic response
bytes and hashes are retained separately from the unavailable original body.
The 53 decoder cases contain no real prompt, response or Case material.

### Required diagnostic handoff

The actionable Core handoff is an observability gap, not a confirmed incompatible
validator: preserve the bounded static validation cause before the generic
classification at the original outcome boundary. Do not log raw parser messages
that may contain argument keys, model text or credentials. Keep HTTP status,
request/response identities, sanitized media type, exact received entity bytes
and digest, framing/completion posture, and parser category/offset separately
from semantic acceptance. Native-call counts, argument extents and schema
identity can be retained without their contents. Producer-side sent-entity
metadata must not be confused with client-received bytes or normalized JSON.
These retention improvements are requirements, not implemented capabilities.

If future byte-level replay is required, it needs an explicitly governed,
bounded private response capture; a hash alone cannot reconstruct bytes.
No new request is authorized to replace the failed submission. The original
Work remains indeterminate and unchanged. No YVEX production defect is proven
by this incident, so no speculative producer repair or installed rollout follows.

Exact source excerpts, fixture runners, dependency lock, source hashes, synthetic
bytes, 16 filtered original journal rows and receipts are retained under
`/home/dgmothx/lab/models/evidence/yvex-response-forensics-20261016.5Br35h`.
Original metadata receipt SHA-256:
`e130c9eff0d332eaea6de5638d5dac334d339eaab1e4aaf9860e81201f452ea4`.
Program P's source, resident services and GPU ownership are untouched. The
provider Task's real-response/YAI acceptance gate remains open.

## Invocation whitespace and structural refusals — 2026-10-17

The subsequent generation-34 failure is distinct from r32 and the generation-31
HTTP-200 incident. Original journal metadata correlates `http-1749`,
`oa-000000000013/r38`, 9,319 input tokens and 91 generated tokens with HTTP 400.
The retained 150-byte error entity has SHA-256
`ec4166cd0d9aac8b2efa5635e6adc9feadac4a665a12bdcf1eb65a0e2fab4512`.
The original completion and failing parser predicate are unavailable. Matching
the static error entity locates the invocation parser; it does **not** identify
which invocation predicate failed or prove malformed model behavior.

A separate, independently reproducible source defect is now established.
The authenticated DSpark `encoding_dsv4.py` identified above parses the
invocation name with `^\s*name="(.*?)">\n$`. YVEX instead consumed the entire
literal `<｜DSML｜invoke name="`, rejecting source-valid zero, multiple, tab,
newline and Unicode whitespace separators. The official decoder accepts all
18 tested separators with unchanged function and arguments, in chat and
thinking modes; zero-width space, a foreign attribute prefix and BOM refuse.
These are synthetic nonempty calls, not a reconstruction of the failed Work.

The tokenizer now applies the existing Unicode classification at that single
lexical slot of the compiled typed-attribute grammar. Canonical serialization,
mandatory delimiters, parameter bytes, offered-function validation, duplicate
refusal and JSON validation remain unchanged. XML-element grammars retain exact
literal matching. There is no ABI, wire, conversation-policy, artifact or binding
identity change and no HTTP-owned second parser.

Invocation errors retain `YVEX_ERR_FORMAT` and the existing diagnostic prefix,
adding a static predicate and invocation-relative byte offset. The diagnostic
does not interpolate generated names, arguments or reasoning. Allocation
failure remains a distinct error. This makes future failures inspectable; it
cannot recover the missing predicate of r38.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Before-fix witness | Existing native parser | New variable-whitespace invocation test | Source-valid call succeeds | Refused; receipt `7fbf8dc5…` | Exact native result | FAIL | Demonstrated false rejection before correction |
| Independent source | SHA-bound official Python decoder | 18 separators × chat/thinking; 3 invalid separators × 2 | Exact function/argument recovery or refusal | 36 accepted, 6 refused | Exact strings; no model execution | PASS | Upstream decoder permits the corrected lexical forms |
| Native completion | DSpark and independently resolved 0731 policies | 108 positive cases; 30 structural/privacy negatives, plus existing regressions | Equal output identities; transactional refusal | `b65e6771…` | Exact arguments, identities and predicates | PASS | Source-compatible parsing and privacy-bounded errors |
| Buffered/SSE | Production adapters and native parser with disposable local Host | 48 source cases, 52 Unicode cases, 3 feedback continuations; pinned SDK controls | Valid calls preserved; invalid calls never finish successfully | `519b7d71…` | Exact calls, arguments, finish classes and predicates | PASS | Native-source-to-HTTP fixture behavior, not model inference |

The first HTTP gate was BLOCKED (`341eafbe…`) because `node`/`npm` were absent
from PATH. The unchanged registered gate passed using the already installed
Node toolchain; no producer or test condition was weakened.

Broad source-stable controls at parent `f4e9bb55…`, dirty delta `d570a7cd…`,
pass `fast` (104 PASS, zero FAIL/SKIP/BLOCKED/ERROR; `96d43edb…`) and
`structural` (20 PASS at the QA gate level; `8fb186d7…`). The latter retains
the build-contract suite's one platform-dependent inner unittest skip; it is
not an executed backend qualification. Subsequent evidence/Task/Status text
does not change the tested implementation. Hosted QA for the parent source
also passed both lanes; that does not establish hosted QA for this correction.

Final runtime/sanitizer checks use the same unchanged C/tests at dirty delta
`318d5a4d…`; both receipts verify source stability. Their broad environment
includes the preserved Program P work, which this repair does not publish.

| Gate | Expected | Observed | Result / claim boundary |
| --- | --- | --- | --- |
| `runtime` | No registered lifecycle, protocol, product or consumer regression | 49 PASS; zero FAIL/SKIP/BLOCKED/ERROR; `dea382a9…` | PASS at software-fixture scope; SDK credential controls use a disposable verified-owner Secret Service, never the operator vault |
| `sanitizer.runtime` | No ASan/LSan/UBSan finding or fixture failure | Complete two-build lane PASS; `db6f5615…`; 48 source and 52 Unicode cases repeated under each instrumentation | PASS at instrumented CPU/native/HTTP/lifecycle scope, not real-model execution |

The pre-correction tokenizer owner on parent main and clean installed-source
`228cf6dd` is byte-identical (SHA-256 `121a3986e561e27e45d69f21dec30726b444273ff1c49e5c5ab0b464f391846e`).
The corrected owner is `a10e274deaeef12b71806b4e85f5289b133e37925e8d368ccb708758105570b7`.
The complete observed source capture, its HEAD/delta record and owned patch are
retained with the receipts below. Archive SHA-256:
`7fbe755f5aa32c44d1eb54610b791dad303cf328ecec51b4996c9658b2ef9b42`.
This is a reproduction capture of an actual Git-built test snapshot, not a
Git-less release build or an independently qualified deployment executable.

Reproduction uses `unit.runtime_tokenizer` and `integration.openai` through
`python3 tools/qa.py run <gate> --jobs 2`. Source-oracle runner and output are
retained under
`/home/dgmothx/lab/models/evidence/deepseek-invoke-recovery-20261017.oplPye`;
the JSON oracle result SHA-256 is
`26d68eda02909a30985620955efe01ff658d5a3854c6f5ff624f0c59daa3d471`.
The prior bounded journal and 37-case forensic control remain under
`/home/dgmothx/lab/models/evidence/yai-dspark34-structural-forensics-20261017.yvYrGV`.

**Historical causality remains unknown.** This correction does not establish
that r38 used variable whitespace, that the installed producer has been fixed,
or that the real YAI Work succeeds. Installed source `228cf6dd`, the resident
DSpark generation, historical submissions and all GPU work are unchanged by
this repair. A separately authorized rollout and new, independently identified
live acceptance are still required; no historical Work may be silently replayed.
