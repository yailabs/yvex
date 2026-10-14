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

## Evidence boundary

| Lane | Authority / fixture | Expected and observed | Supported claim |
| --- | --- | --- | --- |
| Regression witness | `unit.runtime_tokenizer`; exact empty-body completion and zero-parameter schema | Before correction FAIL (`b6fa2240…`); initial corrected control PASS (`54008bf4…`) | Reproducible false format rejection, independently of GPU inference |
| Source serialization | Authenticated official template and empty argument encoder | Exact completion SHA-256 `477e41b5…` | Agreement with the source-authored empty-body bytes, not full-model quality |
| Native tokenizer | DSpark and independently compiled 0731 policies; none/high/maximum; output/call identities and correlated feedback | Complete `{}` call and exact channel/identity preservation; malformed, incomplete, unoffered and duplicate controls refuse | Bounded native parsing and feedback semantics |
| HTTP / SSE | Actual native parser, local protocol and production adapter against a disposable fixture Host | 24 text/tool/error cases and three feedback continuations; errors after opened SSE have no successful finish or accepted tool call | Buffered/SSE projection and terminal failure preservation, not real model execution |
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
