<!-- docs:metadata
title: Metal2 Main Testing Convergence
id: yvex.evaluation.metal2-main-convergence
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Metal2 Main Testing Convergence

**Published main is merged; Qwen CPU chat and the Metal foundation are preserved; the new testing framework still requires publication.**

[Up](README.md)

`METAL2.MAIN.TESTING.CONVERGENCE.0` starts on 2026-10-06 from
`feature/macos-metal2` at `6e84d28da06f7b92ee3fa9ff05e6fd586070e020`,
tracking its identical published pressure ref. Published main is
`803dd98d4c54d7a26cb3c08def6b350be51b7179`; merge base is
`d79b60f209e3f9ae5e86275136bc7a6ebd95c7b9`, with five pressure-only and one
main-only commits. Pre-existing untracked `target/` is preserved.

Task selection is `6be4fef4e1549e5c80e15166ca0993021d0f3fb4`. Normal merge
`b3222de2c9c6041fa195476bc533a0c4f722e30f`, tree
`da7dd3e2aa27d5397964bcc78a473ef001a129ca`, retains both published histories,
including refoundation `f03e2141` and the completed exact Qwen conversation.
Only TASKS counts/selection prose conflict: semantic union preserves main's
completed remote finite-decision Task, all seven Metal2 boundaries and the
new convergence Task. Implementation has no merge conflicts. No rebase, reset,
force push or main checkout edits occur.

## Published main delta and authority

The only main-only commit is `803dd98d`, the scoped remote finite-decision
producer. It does not publish the new model qualification program.

| Class | Published change and effect |
| --- | --- |
| A — architecture/runtime | Versioned finite-decision JSONL over restricted SSH and generated native C FFI; no model/backend numerical execution change. |
| B — model-testing methodology | No new model qualification framework in this commit. Its synthetic remote peer explicitly excludes model correctness and quality. |
| C — QA/evidence infrastructure | Registered `integration.finite-remote`, bounded resources/tool requirements and the `remote-finite-producer` obligation. Common receipt/source-stability semantics remain unchanged. |
| D — project-control/documentation | Completed producer Task, public contracts/JSON schemas, interfaces, commands, evaluation and Status projection. |
| E — other implementation | Rust producer/FFI, restricted compute enrollment and operator dispatch. Published main CLI changes are retained; no additional chat/REPLAI changes. |

The inherited published authorities remain [QA](qa.md),
[benchmark methodology](benchmarks/methodology.md), `tools/qa.py`,
`config/qa/registry.json` and `config/qa/obligations.json`. Receipt schema remains
`yvex.qa.evidence.v1`: independent references, model behavior, numerical
conformance, runtime lifecycle, physical backend execution and performance are
separate evidence classes. A synthetic peer or plausible response does not
qualify a model. Missing evidence never becomes PASS.

All future Metal2 Tasks consume common qualification and promotion authority;
Metal adds physical execution evidence. Full-model Metal must qualify the same
exact model under that common methodology, not define its own model PASS.
Historical [Qwen conversation](qwen-small-conversation.md),
[Metal foundation](macos-metal.md) and [main integration](macos-main-integration.md)
records retain their original source scope. They are not relabeled as results
from an unpublished replacement framework. The seven-boundary program is not
redesigned: Qwen conversation stays COMPLETE, six successors stay READY.

## Publication gate

Live inspection of primary Spark main finds HEAD/upstream at `803dd98d`, zero
unpublished commits and 190 modified/untracked paths. New owners exist only
there, including `tools/qualification.py`, `qualification_gguf.py`,
`qualification_reference.py`, `qualification_run.py`,
`tests/test_qualification.py`, `docs/decisions/0010-qualification-targets.md`,
qualification rule/target schemas and QA/control changes. Their moving bytes
are neither copied nor treated as canonical published authority. The operator
confirms that publication will follow. Before final qualification recovery,
Spark becomes unreachable on both VPN and LAN; Tailscale reports offline with
last-seen `2026-10-06T13:20:00.1Z`. CI/native receipts had already completed,
but the started Linux sanitizer campaign cannot be recovered. Inspect its
owned process/receipt before any rerun; no remote service or network setting
is changed to bypass this gate.

The convergence Task cannot close until that history is published, merged
semantically, its actual changed contracts/owners are inspected and its required
campaign passes or exact mandatory external gates are retained. The current
published integration is a qualified intermediate checkpoint. No new Metal
implementation starts. `METAL.RUNTIME.ADMISSION.0` is the next implementation
after convergence, not selected by this partial exit.

## Intermediate checkpoint qualification

Both qualification copies are clean and unchanged at `b3222de2`: Mac
`/private/tmp/yvex-m2.5gmh7a35`, Linux `/tmp/yvex-m2.fhxk18aq`. Registry/change
plans use both the pressure starting HEAD and common merge base. Existing CI
covers all fast/structural/runtime IDs and all ordinary numerical IDs; the
remaining registered independent DeepSeek encoding and sanitizer controls
require Linux asset/toolchain access. Sanitizers are started there, but the
final receipt and independent encoding rerun remain unavailable after transport
loss. Prior passes remain historical, not new combined-source PASS. Actual Metal qualification uses the registered device lock. The Mac
operator host/chat and primary Spark source/services are not interrupted.

The first Mac CI receipt has 120 PASS and two SSH FAIL. Its qualification cache
was under `/tmp`, whose world-writable/symlink ancestor violates SSH
`StrictModes` and the test cleanup contract. Focused reruns use an owned private
home cache and pass both tests. Authentication policy, production and fixtures
are unchanged. All 122 CI IDs have passing stable-source coverage across these
receipts; this is not one all-green aggregate. Original failures remain retained.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mac CI + focused SSH retries | Common registry and exact product/refusal contracts | 122 registered IDs; owned private cache for retries | Preserve every CI contract | 120 initial PASS, 2 initial FAIL; both focused retries PASS | Exact assertions; stable clean source | PASS coverage, not one green aggregate | All 122 software IDs covered; initial diagnostics retained |
| Linux CI | Same canonical registry | 122 registered IDs | Preserve common platform/producer behavior | 122 PASS; receipt completed before transport loss | Exact assertions; stable clean source | PASS | Linux software and CUDA export/no-NVCC controls; no new model CUDA claim |
| Mac/Linux native | CPU/host/terminal contracts | 15 IDs per host | Preserve native CPU and Rust/REPLAI lifecycle | 15 PASS on each | Exact state/refusal/termios | PASS | Updated product shell and platform compatibility |
| Qwen prompt/reference | Exact pinned template; independent Jinja2/HF tokenizer | 8 representative conversations × 3 rendering modes; malformed/authority controls | Same source-authored bytes and token IDs; invalid authority refuses | 24 exact comparisons; five malformed and four authority refusals | Exact bytes/IDs and refusal statuses | PASS | Existing exact CPU conversation grammar |
| Qwen raw CPU | Retained authenticated source-precision PyTorch/Transformers oracle | Two prompts × eight greedy tokens | Preserve upstream-bounded continuation | 16/16 IDs and both domain-separated text facts agree | Exact prefixes/decoded payloads | PASS | Raw completion regression only |
| Qwen CPU product chat | Same authenticated artifact/binding; typed host/session facts | Ordinary load/chat, two same-session Ada turns | Real responses and committed history | `Ada`, then `Ada`; positions 24→46, turns 1→2; cleanup/termios pass | Exact expected IDs `[92055,92055]` and counts `[23,45]` | PASS | Normal multi-turn CPU product path preserved |
| Metal foundation/failure | Independent host row bits and addressing; admitted ownership/fault contracts | Real M5 Pro F32 embedding plus existing failure controls | Real GPU execution, fail-closed faults, complete release | 12 dispatches, 36 completions, zero bit mismatches; 48 allocations/releases | Exact F32 bits and resource states | PASS | Existing primitive foundation, not a Metal model |
| Mac sanitizer fallback | Registered ASan/LeakSanitizer/UBSan Make targets | Two selected runners | Complete requested sanitizer set | Both stop at compilation: Apple Clang rejects `-fsanitize=leak` for arm64 Darwin | Mandatory requested flag unavailable | FAIL; no sanitizer claim | Existing platform/toolchain limitation; no weakened gate |
| Linux sanitizer / independent DeepSeek encoding | Registered mandatory controls | Started sanitizer campaign; exact existing source assets on Spark | Recover both sanitizer results and run registered reference | Final sanitizer receipt inaccessible; reference not rerun | Not measured | BLOCKED | No combined-source PASS for these gates; historical evidence preserved |
| New main qualification framework | Future published main contracts/rules/test owners | Framework still only in moving Spark files | Merge actual published history and qualify canonical new owners | Required history absent from GitHub | Not executed | BLOCKED | Methodology convergence remains open |

The real Qwen product proof uses normal `model load qwen3.5-0.8b --ctx 256` and
Rust/REPLAI `chat --session qwen-conversation-proof --max-new-tokens 16` on an
owned host. Turn one, “My name is Ada. Reply with my name only.”, returns `Ada`;
turn two, “What is my name? Reply with my name only.”, again returns `Ada`.
Generated IDs are `[92055,92055]`, prompt counts `[23,45]`, positions `[24,46]`
and committed turns `[1,2]`, with the same session identity
`1fd7159e23b03388540057a921b0b3f97311b68cb163d39bf81614d9db8f1dfc`.
Typed events/session queries synchronize the proof; terminal output is not an
integration mechanism. Termios is restored, model unload/session close pass
and the owned host exits zero.

The exact existing artifact remains SHA-256
`0c5776eb6b1f2abb3a35f2324aabc4d8b7693856650b799e88161f7167feded6`,
with admitted conversation binding `dd5b2dbd…`. No registry, payload or binding
is regenerated. Retained independent PyTorch/Transformers source-precision
oracle SHA-256 is `119a0a08bdd69f40735fcaa853e2cb18edad63a4130f9a6090c8214082434a51`;
all 16 raw greedy token IDs and both length-delimited text facts agree exactly.
This is bounded regression, not general conversational quality or full logits.

Metal reports Apple M5 Pro, unified memory, recommended working-set bytes
19069665280 and maximum buffer bytes 14302248960. These are capability facts,
not measured residency. Twelve dispatches and 36 completions have zero F32 bit
mismatches; 48 allocations match 48 releases. After cleanup owned allocated,
mapped and temporary bytes are zero; device allocation is 458752 bytes before
and after. Host write/read copy counts are 760960/3260016 bytes and device copies
1086672 bytes; shared storage does not imply zero host API copies. Physical
residency and actual working set remain unmeasured. No model GPU claim follows.

No C computational, tokenizer, family, Metal or public layout changes occur in
this convergence beyond importing published history. Prior CUDA model evidence
from the unchanged conversation implementation remains historical evidence;
the published delta selects CUDA export/no-NVCC controls, not new whole-model
CUDA generation. Future framework publication may change that obligation and
must be inspected before claiming combined-source model qualification.
Performance is not optimized or promoted. No cross-repository effects or
capability maturity changes occur.

| Stable-source receipt at `b3222de2` | Immutable run identity |
| --- | --- |
| Mac initial CI, 120 PASS/2 FAIL | `274519dea0c56d862097abb5b986eabdd02b6d60368ea5c52a43bbdbe8123ed5` |
| Mac finite remote retry, PASS | `08de95447549c42e83583a71d2aa570e6f5d0ddc9f2fe6a347e2edd4d689e47a` |
| Mac management retry, PASS | `9b046720906aa838eab1dbb2f75ed8f0d74b45b614b7ffb6b30266b5f68a090e` |
| Mac native, 15 PASS | `a8e26231dc093d4a77f0d41f1264bd0cb9ab5f9d7b47ffbd3a46d1d01ccce255` |
| Metal, 2 PASS | `4b3358c279f53acf6a0a1cc070fe8f0e75af2a2ea3e12628d5353ca79d366ff3` |
| Qwen independent prompt, PASS | `fd65ab308807fd12955789ca47ed41805be78a94e029e8141490c9488af4b5a0` |
| Qwen CPU chat, PASS | `939f7a31bb0d0bf5a47001923830043a00aaeadbcba2a2bc5eef57c0d96201ce` |
| Mac sanitizer diagnostics, 2 FAIL | `3593bcb5c5c9a96e99875ba983ff4574728ef877065d1ab01099d4b5b1b96369` |
| Linux CI, 122 PASS | `cd7eb9ffeac464a0e739b609d5dcec27b2b0304c61f4535da9e8381fb184d9e4` |
| Linux native, 15 PASS | `1550ba939cbcbb0220e77c83e6104a750be8bcd2ca8b431ec06f79f48fc40129` |

Raw logs, receipts, chat proof and independent comparisons remain outside Git
under `/Users/mothx/lab/models/evidence/metal2-main-convergence-20261006` and
`/home/dgmothx/lab/models/evidence/metal2-main-convergence-20261006.ik8i6m33`.
Linux CI/native receipt identities were delivered before transport loss; their
raw files remain on Spark and still require recovery. The source/tree, initial
failures and focused retries remain distinct. Final
project-control/evaluation edits do not alter executable code and receive their
own documentation checks.

`progression_decision=blocked_external`; `downstream_safe=false`, scoped to
full methodology convergence. Published-source regression and historical earned
CPU/Metal capabilities are preserved; importing and qualifying the unpublished
main framework is still required before the next Metal implementation.
