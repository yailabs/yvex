<!-- docs:metadata
title: DSpark 32K Runtime Activation
id: yvex.evaluation.dspark-32k-runtime-activation
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# DSpark 32K Runtime Activation

**Previous DSpark resident at 32,768; verifier repaired, no inference dispatched.**

[Admission](../architecture/artifacts-admission.md) ·
[Operator checks](../guides/operator-runbook.md#installed-dspark-32k-observation) ·
[Curated YAI handoff](data/dspark-32k-runtime-activation-20261010.json)

## Result and selected scope

The operator's 2026-10-10 activation extends the installed-profile repair at
the existing deployment boundary. Its verifier, minimal rollout, one admitted
32K load, Exon catalog and capacity-only preflight exits are earned. It does
not resume predecessor `.4`, change Program P implementation authority, adopt
0731, close `.5`, or qualify real YAI agentic work. No successor is selected.

The shared inference Host remains the exact installed `337e7e73` executable,
PID 2491. Only the independent management listener was replaced with the
qualified verifier backport, after observing zero management connections and
jobs. No shared Host restart, engine unload, credential change, registry edit,
inference or YAI SEND occurred. The model is left resident.

## Qualification target

| Fact | Exact observed identity / configuration |
| --- | --- |
| Inference Host source / tree | `337e7e73057abd8a784a67eea58ad8a6f079869d` / `14b7fea18275a542739d4d2afaa38f6fd720d407` |
| Host native build / executable SHA256 | `a72b5d98f4361eb97eb051d761a7133268d1f97da66d7543d9fecc7046a0b857` / `eb108d22b68674f92a5430e246723e4e7f0505ae4d0eef01beb590845a8fba7d` |
| Installed verifier source / tree | `c0ccd7f19bdd20f53d11df5943b57b21feec51a9` / `e76256fd23b59b795674f8037f4128a57b82a3c3` (direct child of installed source) |
| Verifier native build / shell identity | `3f1dbbf7cb10861aa0cdc5e49a297b2325d82c896cc2089e9c6aee0c6658023b` / `ab28104a2d77ed9844dc446844e63840901469a34a8eb458395879cacfc5ed5f` |
| Verifier executable SHA256 | `45b0d002955d97faaa8a6a2a0e6ceb329b52ebc0f431504fe37844e0c081b8ed` |
| Target / draft checkpoint | DeepSeek V4 Flash `60d8d70770c6776ff598c94bb586a859a38244f1` / DSpark `62af8fffb2f7030cac4de2f0169f5b8d1101b646` |
| Physical policy | `deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1` (not 0731) |
| Artifact SHA256 / size | `b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f` / 95,050,210,272 bytes |
| Binding identity / file SHA256 | `8cdb4929c523bd42e3fb82fa18ceed0a0a6732d6efd1d852c88398c8d2d6cd5e` / `a2dbc7898e04eba22c4d78315d3f4dbadf1ee48fb454c9e6f8c2e4320072a277` |
| Binding graph | Schema 17; 1,409 tensors; 43 target and 3 draft layers |
| Hardware / compiler | Spark `spark-7c3d`; NVIDIA GB10/ARM64; CUDA 13.0, `sm_121`; driver 580.159.03; Rust 1.98.1 |
| Engine / strategy | CUDA text engine, `speculative`, context 32,768, prefill chunk 512, physical sequence width 1 |
| Engine generation / state | **1**, `loaded`, `execution_ready=true` |
| Runtime model identity | `cf8ebc69dae8e38f96a47418e8efcebdc47bc4ed05bac76d6974ccc1379e0e7e` |
| Specialization identity | `3fb4ce2033294ae726603f187d8538eff314b0c3f383977d2616d11c9aa29144` |
| Capacity-plan identity | `e63ef1f1e2e80c5941c041b171d00f43049517c13d00ac9e418d68dfaffda2f0` |
| Exon application endpoint | `http://127.0.0.1:18001/v1`, approved SSH carrier to Spark `127.0.0.1:8001` through LAN `192.168.1.70` |
| Public model ID | `deepseek4-v4-flash-dspark-deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1-cuda` |
| Tokenizer identity | `68f23b5e24f8ee3aa208a424041c306581435a0f9eaaf7da7e81d5527af0e50e` |
| Active inference/session/Work | Zero at final observation; no model forward in this delivery |

The versioned verifier installation is
`/home/dgmothx/.local/lib/yvex/c0ccd7f19bdd20f53d11df5943b57b21feec51a9`.
The old installation and authenticated artifact/binding are retained. Registry
SHA256 stays `9520a09d4d81aae180e1bec38bc1807e212c6d053798418b0063b683ab79452f`;
its stored context remains 4,096. The load-only override does not rewrite it.

## Root cause and minimal rollout

`metadata_support()` reconstructs `selected-tensor-materialized` from structural
GGUF facts. The old `yvex_model_registry_compare_metadata()` compared that label
literally with registered `generation-ready`: one false drift, with matching
digest, extent and remaining metadata. The repair compares known later evidence
levels at their common structural projection, without modifying either input.
Integrity reporting no longer copies the stored stronger label into the current
observed snapshot. Unknown/insufficient evidence, changed dimensions and selected
tensor readiness still refuse. Binding, payload, engine, CUDA and reserve
admission are untouched; the public ABI and protocol do not change.

The exact backport contains two production owners, two regression owners and
the admission architecture paragraph. The ARM64 CUDA build was made from a
clean direct child of `337e7e73`, not from current compiler main. Native source
inspection establishes that profile verification is consumed by the CLI and
management FFI. The existing Host loader already uses independent authenticated
profile/binding and engine-capacity admission; it does not need the repaired
metadata string comparison to load this profile.

Supported `make install prefix=...` creates the new versioned installation.
The only service override is
`/home/dgmothx/.config/systemd/user/yvex-management.service.d/32k-verifier.conf`,
which changes management's `ExecStart` to that installation. Management PID
3615 became 824841; inference Host PID 2491 and finite SSH listener PID 3616
were unchanged. No TLS identity, enrollment, grant or credential was replaced.
The management pin remains
`tls-sha256:df09a3fb3d3158adb0e7f2ef3bfcef9810b74a461498854ac52f9ada2545e293`.

The real management `profile.verify` Job
`d8c44833c61ac0cf2b0b4c9835da44525976f1cd87026d40aa7a799550d160e9`
was observed by exact `job.get` to `succeeded`: identity, metadata and selected
readiness PASS. This does not convert its structural readiness check into a
model-execution result.

## Capacity, load and actual memory

Immediately before the first dispatched load, the installed capacity owner
rechecked the authenticated binding with the exact selected geometry:

| Quantity | Bytes | Evidence kind |
| --- | ---: | --- |
| Required pre-residency peak | 116,211,456,040 (108.2304 GiB) | Planner estimate, including reserve |
| Available capacity | 126,519,549,952 (117.8305 GiB) | Fresh sampled capacity, not reservation |
| Safety reserve already included | 16,332,895,744 (15.2112 GiB) | Existing maximum of 8 GiB and one-eighth system memory |
| Remaining estimated margin | 10,308,093,912 (9.6002 GiB) | Arithmetic difference; no future-availability guarantee |
| Model mapping / device-addressable span | 95,050,210,272 (88.5224 GiB) | Admitted mapped artifact, overlapping byte classes |
| Final sampled Host RSS | 94,552,313,856 (88.0587 GiB) | Process observation including mapped pages |
| Host RSS peak | 95,341,666,304 (88.7938 GiB) | Retained process peak, not separate model memory |

All Host cgroup ancestor limits were unbounded at observation; no competing
engine/session or observed compute process was present immediately before load.
These samples are not an uninterrupted GPU reservation. The load owner performs
its own live capacity admission; no reserve or context was reduced.

One initial CLI invocation incorrectly supplied `--json` to `engine load`.
The current public grammar rejects that flag before client connection
(exit 2), with zero engine/load counters. It was **not a dispatched failed load**.
After a fresh capacity observation, the first actual dispatch used the supported
grammar:

```sh
/home/dgmothx/.local/lib/yvex/c0ccd7f19bdd20f53d11df5943b57b21feec51a9/bin/yvex engine load deepseek4-v4-flash-dspark-deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1-cuda --ctx 32768
```

This operator-authorized command completed with exit 0. Caller timestamps span
18:59:34–19:01:08 UTC on 2026-10-10; the native residency stage reports
92.713969454 seconds and 1,409/1,409 tensors. These are one-load observations,
not a performance benchmark or SLA. A concurrent read-only engine-list query
timed out during residency; the load was never redispatched. Later native and
HTTP observations report the same successful generation.

Placement is `mapped-device-addressable` on UMA. Explicit host/device model
allocation and prepared bytes are zero; the addressable span is not zero GPU
use. `physical_residency_known=false`: exact physical GPU page residency is not
measured. Do not sum mapping, addressability, process RSS and page cache, or
treat post-load `MemAvailable` as spare warm-model headroom. Session/attention
and request workspace were zero because no inference was performed. Runtime
can hold eight logical sessions but this profile admits only one physical
sequence; eight-way execution is not claimed.

## Public preflight and YAI handoff

From Exon, `/health` and `/v1/models` return HTTP 200; the latter exposes the
exact generation, artifact, binding, specialization and capacity above. The
normal installed `yai provider models` independently consumes that same catalog.
Its existing stored provider qualification still declares `text_to_text` only;
reachability and catalog observation do not add native-function qualification.

Capacity-only requests were made to
`POST /v1/chat/completions/preflight`, using the discovered exact model,
`yvex_engine_generation=1`, `reasoning_effort=none`, `seed=32`, `stream=false`,
`max_tokens=256`, one `function` with `strict=false`, `tool_choice=auto` and
`parallel_tool_calls=false`. They never called Chat Completions for inference.

| Controlled request shape | Exact input tokens | Rendered bytes | Requested / effective output | Outcome |
| --- | ---: | ---: | ---: | --- |
| User request + function definition | 295 | 1,370 | 256 / 256 | Compatible, full requested output fits |
| Matched assistant function-call history + tool feedback | 371 | 1,757 | 256 / 256 | Compatible, full requested output fits |
| Near-limit capacity input + same definition | 32,290 | 161,338 | 256 / 256 | Compatible, full requested output fits |

The curated handoff retains exact prompt and provider-request identities.
The capacity workload is synthetic input to the real tokenizer; assistant/tool
history is explicitly controlled, not model-generated tool use. Final counters
remain zero sessions and zero completed/failed/cancelled model requests, with
one model open and one residency build. Preflight claims
`execution_or_resources_qualified=false`, `resource_reservation=false`.

**32,768 is the total sequence envelope, not 32K input plus 32K output.** Input
includes tokenizer/template/tool overhead. Output follows
`ceiling_clamped_to_remaining_sequence`; each fresh invocation must repeat exact
preflight and current-generation/resource admission.

The installed compatibility profile declares native function definitions/calls,
matched tool-result feedback, stateless reconstructed multi-turn Chat, buffered
output and HTTP/SSE. `strict=true` is unsupported. This delivery establishes
input-shape/tokenizer compatibility only. Actual tool emission, consumption of
feedback, next-model-step behavior, SSE delivery, quality and inference latency
on this generation remain **NOT RUN**. YVEX never executes application tools.

YAI's next independently authorized qualification must bind the observed model
and generation through its existing provider owner, qualify native calls and
feedback with fresh immutable submissions, and retain Authority/Review/Receipt
and terminal Work evidence. No historical Tech Infra SEND may be replayed.
No YAI/SDK/Studio public contract or source was changed here.

## Qualification and limits

| Lane | Exact scope / oracle | Result and supported claim |
| --- | --- | --- |
| Native backport build | Clean `c0ccd7f1`, ARM64/CUDA 13.0 `sm_121`, Rust 1.98.1 | PASS; exact installable binary identified |
| Native unit regression | Full unit suite, exact runtime-binding filter; structural support, drift, integrity, graph/binding/replacement, generation and reserve controls | PASS; no admission/readiness weakening |
| CLI integrity | Artifact metadata, artifact integrity regression and integrity-report scripts | PASS; structural projection and real drift refusal |
| Native ownership / ABI / docs | Project control, docs/publication, C/C++ ABI, C/Rust ownership and architecture | PASS on the exact installed backport; 40 public records unchanged |
| Actual profile | Full 95-GB audit through CLI and management Job | PASS digest/extent/metadata/selected readiness, not inference |
| Supported native load | One actual selected load, then native/HTTP observations | PASS generation 1, loaded/ready, correct geometry and identities |
| Exon consumer | Health/catalog, official YAI provider observation, three preflights | PASS reachability and exact tokenizer/capacity compatibility only |
| Main integration candidate | Repair merged onto `75fe8220`; CPU build, affected units/CLI, docs registry/publication, project control, ABI and source ownership | PASS at stated scope; 45 existing main ABI records, no public delta from repair |
| Main aggregate docs gate | Unchanged main README and unchanged docs-surface test | FAIL: test expects retired `## Product boundary` heading; baseline README has an explicit HTML anchor instead |
| Inference/model quality/performance | None/high/maximum, target-only/speculative, stream and native functions | NOT RUN; no new numerical or competitive claim |
| Real YAI Work / human acceptance | No SEND or effect authorized by this activation | NOT RUN; independently owned exit |

No full aggregate QA PASS is claimed. The main documentation mismatch predates
this repair (`git diff 75fe8220 -- README.md tests/test_docs_surface.sh` is empty)
and is outside the minimal verifier/runtime activation boundary. The mandatory
native installed-backport lanes passed; missing broader QA and model evidence
are not promoted by those results. The earlier Exon runtime-binding memory
failure is superseded only for the exact native full-unit proof, not erased.

## Publication, preservation and rollback

Backport branch `delivery/dspark-32k-verifier-20261010` publishes `c0ccd7f1`.
The main-compatible delivery merges that history into `75fe8220` at
`3000d946dde12ec2c684179aea62b2a20743e069` and adds this evidence/control/runbook
closure. That newer documentation/integration revision is **not** the installed
inference Host or verifier binary. Compiler WIP on Spark, the shared Exon
checkouts and YAI/SDK/Studio work remain untouched. No benchmark projections or
capability counts are changed: this is activation, not competitive promotion.

Native scratch evidence is retained at
`/tmp/yvex-dspark-activation.I1mFlwtd` on Spark; Exon source, consumer records and
qualification logs at
`/home/mothx/.cache/tmp/yvex-dspark-activation.lmRsxIhf`. Neither raw runtime
dumps, operational registries, builds, weights nor credentials enter Git. The
published handoff is a curated delivery record, not a new public API schema.

No rollback is needed after the successful load. If the verifier needs rollback,
first confirm management has no active client/job, move the single owned
`32k-verifier.conf` aside, daemon-reload and restart **only management** to its
original unit. The original versioned installation is preserved. This leaves
the inference Host and resident generation untouched. Shared Host restart or
model unload still requires explicit operator authority; neither is an
automatic rollback or a reproduction check.

`progression_decision: complete_evidence`

`downstream_safe: true` — exact 32K resident deployment, catalog and capacity
preflight; not real-model native tool competence, YAI agentic closure, latency,
independent quality, 20/700 performance, or release.
