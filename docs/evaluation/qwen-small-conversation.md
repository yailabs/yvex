<!-- docs:metadata
title: Exact Small-Qwen CPU Conversation Qualification
id: yvex.evaluation.qwen-small-conversation
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Exact Small-Qwen CPU Conversation Qualification

**The exact source-qualified Qwen3.5-0.8B checkpoint is an admitted conversational model through the normal YVEX CPU runtime and product chat path.**

[Up](README.md)

## Source and qualification boundary — 2026-10-05–06

`QWEN3.5.SMALL.CONVERSATION.ADMISSION.0` starts at
`f03e21414e854681381896c2f721a5664007e851` on `feature/macos-metal2`, in
`/Users/mothx/Developer/YAI/yvex-metal`. The implementation is
`b2faa90732b30063f240d7d443e48740d3aded0f`, tree
`1d075f0363ea0a80f04dc274a491b38d28236a47`. Its final model, prompt and regression
receipts use clean, unchanged disposable sources. `7501a83e908242e12077e63ec9ecce04f00836ae`
changes only QA obligation scope and the DeepSeek prefill fixture; production,
headers and build behavior are identical to `b2faa907`. The repaired DeepSeek
and final Linux native receipts bind that second clean commit. Closure changes
documentation only.

Published `main` moves from `d79b60f2` at selection to
`803dd98d4c54d7a26cb3c08def6b350be51b7179` at final remote inspection. This Task
does not merge it. Primary Spark has additional unpublished work and remains
untouched. Qualified sources are `/private/tmp/yvex-qwen-qa.9j7hQm` on Mac and
`/tmp/yvex-qwen-qa.jhgz2K` on Spark. The existing operator Mac host is preserved;
tests own separate short socket namespaces, registries, sessions and hosts.

### Authenticated conversation authority

The immutable source is
[Qwen/Qwen3.5-0.8B@2fc06364715b967f1860aea9cf38778875588b17](https://huggingface.co/Qwen/Qwen3.5-0.8B/blob/2fc06364715b967f1860aea9cf38778875588b17/tokenizer_config.json).
Its authority is `tokenizer_config.json#chat_template`, not the larger checkpoint's
protocol. A pinned fresh read confirms the revision header, Git blob
`fae3ce993e07c092ad024dde45e592379fde91bb`, full file digest and decoded template.
There is no separate `chat_template.jinja` or `generation_config.json` at this pin.

| Authenticated fact | Exact value |
| --- | --- |
| Small source revision | `2fc06364715b967f1860aea9cf38778875588b17` |
| Tokenizer config SHA-256 | `49e2b6e395f959f077f1e992b338919c0d4a9732fc6e613995e06557f843500c` |
| Decoded UTF-8 template SHA-256 | `273d8e0e683b885071fb17e08d71e5f2a5ddfb5309756181681de4f5a1822d80` |
| Tokenizer JSON SHA-256 | `5f9e4d4901a92b997e463c1f46055088b6cca5ca61a6522d1b9f64c4bb81cb42` |
| Model config SHA-256 | `b90b86f35c8e6925ef74ee04d0e758f0a845c83a42089ad82bbaa948de9b4204` |
| Larger source, retained separately | `1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0` |
| Larger template, not small authority | `c3cf9e34abf4f9e36c2d72165aa9c132d3e2a725b6c2586aaa3a8af9d7a81041` |

Small vocabulary size is 248070, with 248044 base tokens, 247587 merges,
26 added tokens and 14 special tokens. Model-generation EOS is 248044
(`<|endoftext|>`); tokenizer/conversation EOS is 248046 (`<|im_end|>`), and pad
is 248044. `<|im_start|>`, `<think>` and `</think>` independently encode as
248045, 248068 and 248069. These facts remain distinct: the source-owned raw
greedy model policy is retained, while conversational stopping consumes the
admitted tokenizer policy through the existing runtime.

The common renderer reproduces trimmed role framing, an explicitly empty system
message, prior assistant reasoning removal before the latest real user query,
consecutive tool-result envelopes and generation prompts. A wrapped tool result
is not a user query. Default chat disables thinking with the source-authored
closed thinking prefix; explicit thinking uses the open prefix. The small source
does not inherit the larger source's low/xhigh effort instructions. This is
bounded text conversation, not multimodal or universal template support.

### Architectural delta and immutable preparation

Family authority names the exact target, revision, template hash and compiled
conversation policy. The common tokenizer authenticates full asset bytes, then
the decoded template and revision, and reconciles any redundant standalone
template rather than trusting a filename. Missing or mismatched authority leaves
no admitted runtime plan. Compilation selects the small adapter version 4;
the larger target retains version 3. Runtime/session consume the sealed policy.

The existing GGUF has no standalone `tokenizer.chat_template` entry but retains
the complete authenticated tokenizer config. Its historical
`verbatim-qwen3.5-0.8b-v1` creation metadata remains truthful. Rebinding validates
that immutable creation recipe, while the new binding authenticates current
conversation semantics. Canonical no-calibration `none` is treated as absent
calibration, and preparation recovers and verifies the catalog-selected acquired
source rather than a constructed path. A new binding-qualified profile preserves
the old profile and binding instead of overwriting them.

- Source model, tokenizer asset bytes, all 320 tensor payloads and physical layout
  are unchanged: 284 BF16 and 36 F32, with one tied embedding/output parameter.
- The 1,528,566,432-byte GGUF retains full SHA-256
  `0c5776eb6b1f2abb3a35f2324aabc4d8b7693856650b799e88161f7167feded6`.
  Physical policy identity remains
  `12080587185f1286503749fe157e05599aa8ea8d438e1608d76dcfe1092b283d`.
- Conversation policy and compiled semantic/binding identities legitimately change.
  New binding identity is
  `dd5b2dbd8ada6a405854fd857951336601f52ace301e2f5c5cc7e922b4d99126`;
  its 390624-byte file SHA-256 is
  `d4094ebd6b56799f8fe809726aa3b6a01decfb09c439cab249dfedd7971179a8`.
  Retained raw binding `c7a79f47…` is not rewritten.
- Normal `model prepare` publishes/selects the new CPU profile
  `qwen3-5-0-8b-qwen3-5-0-8b-source-faithful-cpu-dd5b2dbd8ada6a40`.
  A repeated clean-source preparation is `READY`, `changed=false`; no new tensor
  representation is emitted. The same source-faithful policy document is restored
  alongside the retained physical plan through the ordinary policy owner.

Material implementation owners are `model.families.qwen3_5`,
`graph.families.qwen3_5`, `tokenizer.execution`, `tokenizer.prompt`,
`app.preparation` and `graph.binding_compile`, plus the existing internal family
header. Tests and registered Make/QA owners supply the qualification. No public
ABI or wire version changes, new production files, alternative runtime, Rust CLI
change, REPLAI change, Metal change or cross-repository modification is required.
The existing architecture represents the authenticated source; no new ADR is
needed. Generation/representation architecture and the runtime contract change
with the implementation.

## Independent references and real product evidence

The reference executes the pinned template with Jinja2 3.1.6's immutable sandbox
and the independent Hugging Face Rust tokenizer 0.23.2. Eight conversations in
chat, thinking and transcript modes yield 24 exact byte/token-stream matches.
They include history, system/empty-system, Unicode, reasoning removal and tool
results. Five malformed source conversations are rejected by both source and
native owners; four revision/template/config/policy authority mutations refuse
with no sealed plan. Native unit controls add orphan assistant/tool and invalid
UTF-8 refusal. Numerical criterion is exact bytes and token IDs, not response
plausibility.

The standard host, `model load qwen3.5-0.8b --ctx 256` and unmodified Rust/REPLAI
`chat --session qwen-conversation-proof --max-new-tokens 16` execute two real
greedy CPU turns. Typed host events and session queries synchronize the proof;
human terminal output is retained, not parsed for integration decisions.

| Turn | User input | Actual model response | Prompt tokens | Committed position / turns |
| --- | --- | --- | ---: | --- |
| 1 | My name is Ada. Reply with my name only. | `Ada` (token 92055) | 23 | 24 / 1 |
| 2 | What is my name? Reply with my name only. | `Ada` (token 92055) | 45 | 46 / 2 |

Both use session identity
`1fd7159e23b03388540057a921b0b3f97311b68cb163d39bf81614d9db8f1dfc`, the
same CPU engine and exact artifact. The second prompt includes committed history;
this is not two unrelated one-shot requests or a claim about incremental prefix
reuse. Each response commits one generated token and reaches ordinary EOS.
`/quit` restores termios, session close/model unload succeed and the owned host
exits 0. Final GPU cancellation/reset/terminal composition remains the later
`METAL.QWEN.CHAT.0` boundary.

Raw CPU generation is replayed at the clean implementation against the retained
independent source-precision PyTorch 2.14.1 / Transformers 5.18.0 oracle from
[original small-model qualification](macos-small-model.md). All 16 greedy tokens
match: Italy `21047,13,198,760,6511,314,9338,369`; code
`198,262,460,264,478,292,271,727`. The upstream model oracle is retained rather
than recomputed here. These exact bounded prefixes do not establish full logits,
general conversational quality or long-context behavior.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Source/template/prompt | Exact pinned config/Jinja and independent tokenizer | 8 conversations × 3 modes, 5 malformed, 4 authority mutations | Exact bytes/IDs and refusal | 24 matches; all 9 negatives refuse | Zero byte/ID differences | PASS | Source-bound compiled text conversation |
| Real CPU product | Typed model/session/trace contracts | Existing artifact, standard load/chat, two Ada turns | CPU response and same-session history | Ada → Ada; 1 → 2 turns; cleanup/restoration | Exact IDs, prompt counts and session lineage | PASS | Normal multi-turn CPU chat |
| Raw CPU | Retained independent source-precision upstream oracle | 2 raw prompts × 8 tokens | Historical continuations retained | 16/16 IDs and text facts match | Exact prefixes | PASS | Raw regression only |
| macOS CI coverage | Registered CI, immutable implementation | 121 test IDs, serial run plus focused retry | Every affected ID passes | 120 PASS/1 HTTP-stress FAIL; focused HTTP retry PASS | 121/121 IDs have stable successful evidence | PASS with retained failed run | Test coverage, not one green aggregate CI receipt |
| Native Mac/Linux | Existing platform/PTY/CPU contracts | 15 tests per host | Preserve native product | 15 PASS each; natural Mac capacity, declared Linux fixture capacity | No FAIL/SKIP/BLOCKED/ERROR in final native runs | PASS | Native regressions |
| Linux CI | Registered CI, CUDA-disabled common source | 121 tests | Preserve Linux/common behavior | 121 PASS | No FAIL/SKIP/BLOCKED/ERROR | PASS | CPU/platform separation |
| Linux sanitizers | ASan, LeakSanitizer, UBSan | Registered quant and runtime suites | No detected ownership/undefined behavior errors | Both PASS | Sanitizer errors zero | PASS | Affected memory-safety controls |
| Existing Qwen CUDA | Exact admitted 27B binding and internal direct replay | Full 248320 logits, hybrid prefix and stale v16 | Preserve large target; stale binding refused | Finite logits; replay max_abs=0; cleanup | Exact replay, tolerance 0 | PASS | Existing CUDA regression, not upstream conformance |
| Shared prompt regression | Authenticated DeepSeek source/reference | 13 tokenizer cases, 3 prompts, 4 official vectors | Existing grammar retained | PASS | Exact registered criteria | PASS | Existing source/prompt boundary |
| DeepSeek live generation | Current admitted fixture and runtime contracts | CPU/CUDA target and DSpark, greedy/stochastic, lifecycle | Full registered gate retained | PASS after test-only phase-width repair | All existing assertions retained | PASS | Shared compilation/runtime regression |
| Documentation/control | Canonical architecture/publication and QA registry checks | Earned closure records | Valid owners, links, counts and registry | docs-check PASS; 17 publication tests; 52 Tasks; 181 QA tests / 15 lanes; diff check clean | Controlled states and exact counts | PASS | Reviewable project-control closure |

Final receipts have `source_stability.valid=true` and clean sources. Mac evidence
is retained under `/Users/mothx/lab/models/evidence/qwen-small-conversation-20261005`,
including `mac-qa`, `chat-final`, `reference-final.json`,
`special-token-reference-final.json`, `raw-reference-comparison-final.json`,
`artifact-binding-identities.json` and the source download headers. Spark originals
are under `/home/dgmothx/lab/models/evidence/qwen-small-conversation-20261005.VvqWdA`;
receipts/logs are copied into the Mac root's `spark-qa`; `qualification-index.json`
binds retained receipts and the explicit Mac CI test-ID coverage. Assets, registries, raw
traces and build/dependency products remain outside Git.

### Immutable receipt identities

- Prompt/reference: `a8bc515e68300566196f716d66e570605fca86774e73f5fbb66058c4878a4752`.
- Real CPU chat: `da2ab2747f6ba16d714ae66e668e2c3491bf51b6ca596fe8ebeec55536398acf`.
- Mac native: `8e6d31f8a0273df3ea8aea49d172ba294833b7690b14952e1cf4be4b54803770`.
- Mac CI (120 PASS/1 FAIL): `3637f0624ba06e2190c1866815059aa743f27041b18ff13d0159c2528aacb3c5`;
  focused HTTP retry: `a7e78627f8fefcc0ea3722b71bfb5b59fb8485210c7538bbf8a6824ab6725696`.
- Linux CI: `15551c526c9d5603b3567375646489cabf933099ac55877e7fa30b440a47e359`.
- Linux native: `f4fe1c96f1336cca0e12970617fd3908d7b18957e3870a3261b3be030613bdb0`.
- Linux sanitizers: `638a82f299d229fa5afac7c9ad50f1b5f17a151c6e73f5f9fd8ad6306795e7a4`.
- Existing Qwen CUDA: `4ddbdf53033fdf096c7c0353cbacc80ef7a79fd863ec4965ae47b2b9207d7a8c`.
- DeepSeek reference: `1a56d7e296532a1a1de0761778d0b85cf2347749ca39fe8a21778ab972c16ace`.
- Repaired DeepSeek generation: `e7dc8c014b483b6cfc6e231634df2288a6ddb6b4ccaa00f60c8b9945a5fa00ae`.

### Failures retained and correction ownership

Initial CLI fixtures used relative runtime paths and observed the existing
operator host; the fixture now owns an absolute short temporary namespace.
A long Mac qualification root exceeded Unix socket limits and was replaced with
the short disposable root. Overlapping owned qualification initially tripped
the source-safety fence (`6d5cef34…`, 120 PASS/1 CLI FAIL); serialized CLI/native
qualification passed. A later serial CI HTTP stress request reset one connection;
the focused unchanged-source HTTP rerun passed. Neither failed CI receipt is
relabeled as a green aggregate.

Spark initially lacked reachable dependency downloads and PATH tools. Private
authenticated REPLAI sources and official Node/ripgrep tools restored the exact
build in the disposable checkout without changing user installation. Linux
hermetic capacity fixtures explicitly use `YVEX_TEST_FIXTURE_CAPACITY=1`; real
model CUDA gates use actual hardware and canonical GPU locks.

The DeepSeek live fixture incorrectly used global scheduler width rather than
the admitted prefill phase width, and excluded width 1 from clamping. Its failed
receipt `6540bb44…` remains retained. The test-only correction at `7501a83e`
matches the compatible same-owner correction observed in ongoing Spark work;
unpublished runtime changes are not imported. The actual repaired target admits
32 physical rows for a 64-token logical chunk. The full gate passes; no assertion
or production capacity contract is weakened. Family-specific QA obligations now
name the required Qwen gates instead of selecting every unrelated live family;
the explicit shared-IR DeepSeek generation obligation remains mandatory and passes.

## Reproduction, control state and remaining boundary

Registered `reference.qwen-small.conversation` and `live.qwen-small.chat` run via
`python3 tools/qa.py run <test-id>`. Supply exact local source/artifact/current
binding with `YVEX_QWEN_SMALL_SOURCE`, `YVEX_QWEN_SMALL_ARTIFACT`, `YVEX_QWEN_SMALL_BINDING`
and the independent environment with `YVEX_QWEN_SMALL_REFERENCE_PYTHON`; missing
mandatory assets remain BLOCKED. Ordinary preparation uses the admitted
`qwen3.5-0.8b-source-faithful` preset and CPU backend. The generated evidence is
not an alternate product shell.

The conversation Task is COMPLETE. Its next implementation boundary is
`METAL.RUNTIME.ADMISSION.0`, still READY; no later Task is automatically started.
Status records exact CPU conversation without changing generic chat maturity,
Metal maturity or historical raw/publication evidence. Canonical changed docs
are generation/representation architecture, runtime contract, this evaluation,
family scope, evaluation routing, Tasks and Status. No cross-repository change.

An already-running host retains its old engine/binding until the operator retires
and reloads it; updating a profile does not mutate a live engine generation.
Owned test hosts are unloaded and stopped. Physical residency, working set and
peak chat memory are unmeasured; this is not a resource/performance benchmark.

`progression_decision=proceed`, `downstream_safe=true`, scoped only to the exact
qualified CPU conversation prerequisite. No Task blocker remains. Full-model
Metal execution, GPU chat/cancellation composition, long-context/quality and
release readiness remain unqualified independent boundaries.
