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
