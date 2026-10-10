<!-- docs:metadata
title: DeepSeek V4 Flash / DSpark Technical Record
id: yvex.model-families.deepseek-v4-flash
document: reference
status: mixed
owner: model
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# DeepSeek V4 Flash / DSpark Technical Record

**Exact family semantics, admitted execution and remaining evidence limits.**

[Up](README.md)

This record owns current DeepSeek V4 Flash family facts and the exact DSpark
target admitted by YVEX. It does not own macro gate state, operator procedure,
or release promotion.

## Identity and source

| Fact | Value |
| --- | --- |
| Family | DeepSeek V4 |
| Canonical target | `deepseek4-v4-flash-dspark` |
| Source repository | `deepseek-ai/DeepSeek-V4-Flash-DSpark` |
| Pinned revision | `62af8fffb2f7030cac4de2f0169f5b8d1101b646` |
| Conversation encoder | `encoding/encoding_dsv4.py` |
| Conversation encoder SHA-256 | `bdbd57c132a1b3725042323d02b98b9d1df28e5f388f134399555d041f5055e0` |
| Snapshot shape | 48 Safetensors shards; 72,317 indexed tensors |
| Target topology | 43-layer hybrid SWA/CSA/HCA decoder with mHC and MoE |
| Draft topology | five-position DSpark block conditioned by three target feature taps |
| Model maximum context | 1,048,576 positions |
| Hidden width / output vocabulary | 4,096 / 129,280 |
| Routed experts / selected per row | 256 / 6 |

The external acquisition record binds the exact revision, sidecars, tokenizer,
index, every shard header and payload identity, and the ordered aggregate
snapshot identity. YVEX source intake independently produces and consumes its
canonical `yvex.source_manifest.v3` payload manifest before compilation. The
operator record is acquisition evidence, not a substitute input schema; a
directory name, mutable branch, or local modification time is never source
identity.

The superseded `deepseek-ai/DeepSeek-V4-Flash` snapshot and
`deepseek4-v4-flash` target are not current aliases. Living product surfaces
refuse the old target spelling with one migration hint. Historical records and
Git retain its provenance.

## Selected matched-checkpoint candidate

On 2026-10-09 the operator selected Flash 0731 for the initial matched-checkpoint
comparison in `V010.RUNTIME.DEEPSEEK.GB10.COMPUTATIONAL.ARCHITECTURE.5`.
This is a separately identified candidate, not a replacement of the admitted
source above or a claim that the release label proves chronological recency.

The external control uses DwarfStar commit
`9139e2ae58a41503968a500f36f75895c1ba63fc`. Its paired files are distributed by
[`antirez/deepseek-v4-gguf`](https://huggingface.co/antirez/deepseek-v4-gguf/tree/f71f23d552d664e523b422157b2befbf74040380)
at immutable revision `f71f23d552d664e523b422157b2befbf74040380`:

| Role | Distribution file | Provider SHA-256 | Bytes |
| --- | --- | --- | ---: |
| Target | `DeepSeek-V4-Flash-IQ2XXS-w2Q2K-AProjQ8-SExpQ8-OutQ8-chat-v2-imatrix-0731.gguf` | `ca22ae2f838e14077c22bc1c1417b71b45b5e5a3687bd96c2ac6e17fdb6261c0` | 86720111488 |
| Draft/support | `DeepSeek-V4-Flash-DSpark-support-0731.gguf` | `7e319924541db3f7a163ed7e11d7532a70d48228ab59d36cb81e1d4511885360` | 5989114272 |

These identities authenticate the released quantized payloads; their GGUF
metadata does not supply the original upstream revision. The separately pinned
official source candidate is
[`deepseek-ai/DeepSeek-V4-Flash-0731`](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash-0731/tree/7872f01b1d1fe23eabc4c98b48bffcef5a386062)
at `7872f01b1d1fe23eabc4c98b48bffcef5a386062`, catalog target
`deepseek4-v4-flash-0731`. Its index is Git blob
`c3b10d45a829545fbf0d9d2880a1aa0b9ab3b43a` (5,602,871 bytes, 48 shards).
The source catalog does not promote this candidate to the release alias or
installed model. A separate compiler target now owns source reopening,
conversation compilation and physical-variant preparation for this exact pin.
The family architecture interpreter accepts its exact verified source tuple and
derives a distinct model identity. Fixture controls reject a 0731 target paired
with the old repository/revision and preserve the 43 target / 3 support-layer
geometry. This bounded IR admission is not a complete artifact/compiler binding
or a deployment; neither the old transform identity nor calibration is reused.
The source-derived Transformation IR is
`ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d`.
Its initial native preset `deepseek-v4-flash-0731-q8_0-q2_k-v1` has policy identity
`d66294b5e016806f2e283e8b0160e25385302fa6c9db4ac24a49e94140fa0ea9`:
Q8_0 approximable projections and Q2_K routed aggregates, with the recipe's
preserved exact/source classes unchanged. It requires no imatrix. The previous
checkpoint's calibration is not admitted by this compiler pipeline. This is a
different quantization from the external 0731 IQ2_XXS control, not same-weight
engine equivalence. Planned storage and qtype compatibility do not establish
emitted integrity, runtime admission, quality or speed. The isolated preparation
does not replace the installed engine or retarget existing aliases.

The [native preparation record](../evaluation/benchmarks/generated/qualification-deepseek-0731-native-admission-20261009.md)
now records complete emission, native roundtrip, the independent pinned ggml
reader, authenticated binding publication and reopen. Artifact
`4dc4265a92d77c874c82aa16c1358688b71bb7b10267cc80911c3ef42d2fa11a`
binds to
`29676295361a8c99b86b3e1837a64e23791337f862d01fbeaeb2b5361399cc6d`;
the sealed tokenizer policy is
`bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819`.
That Q8/Q2 record establishes preparation, not engine residency, a model forward
or independent quantization quality. Program P subsequently emitted a distinct
`goal-v1-q2_k` artifact (`7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f`)
and authenticated binding
`a4bb99c92e1f8d1fb61f7db3ed0fc5d7a900fcc87b7ecd6fd8c75c486a939737`.
Its native target-only full-model execution is bounded experimental evidence;
the generic homogeneous Q2_K matrix consumer preserves its declared arithmetic
class. This neither requalifies Q8/Q2 nor establishes independent model quality,
a recommended quantization, installed-product deployment or the throughput exit.

Configuration, tokenizer and index bytes match the prior DSpark source, but an
identical index does not authenticate identical tensor payloads. The retained
43-tensor preserved-F32 comparison establishes that the external target differs
from the old admitted DSpark source. The new source's 48 payload shards now pass
native upstream-digest authentication (72,317 tensors, 166,886,535,336 bytes read;
manifest v3 payload identity
`d46c2f3a4305155f357191e0bd804defe33764b92997c3de123b3b5cab41e5ce`).
All 43 preserved F32 attention sinks match the external target. This bounded
relationship does not authenticate every quantized parameter or the support
model; complete checkpoint equivalence remains UNPROVEN. Old checkpoint
reference and representation-quality claims cannot transfer.

The new encoding implementation has SHA-256
`abc0d26120250dda0ae077dc64aa28836026e61e970854aaeb792445e6a0dde6`.
Its four official encoding/parsing cases pass, with unchanged fixture bytes,
but its reasoning policy differs: `high` now uses the previous maximum
instruction and `max` has a distinct upstream instruction. Neither the four
vectors nor tokenizer equality qualifies native high/maximum prompt parity.
The new conversation policy must be admitted independently; reusing the old
maximum instruction or silently mapping the two levels is forbidden.
Checkpoint-owned conversation recipes now reside in the tokenizer family
projection, separate from model topology and generic rendering. Compilation
selects an exact target and seals its pointer-free policy; runtime does not look
up a family. Three bounded `hello` request controls (none/high/maximum), derived
from the authenticated 0731 encoder, match native/provider prompt bytes. They
supplement the official vectors. The emitted artifact and compiled binding now
also pass all four official BPE controls, one upstream request-prefix control,
13 supplemental BPE controls and four supplemental prompt controls spanning
none/high/maximum. This does not qualify complete tool/history projection,
model execution or quality. The old checkpoint's
prompt digests remain unchanged.

The `deepseek_0731_official_encoding` entry in `tests/vectors/manifest.json` owns the new
upstream input hashes. `tests/vectors/deepseek_0731_product.json` preserves the
previous suite's logical cases under this separate checkpoint authority.
Thirty case/mode inputs are prepared with the upstream encoder; independent
full-model outputs remain absent. The bounded native Q2_K execution described
above is separate evidence, not an independent reference continuation.
The separate `deepseek_0731_prefill.json` and `deepseek_0731_competitive.json`
suites retain the earlier logical workload bytes under the new checkpoint
authority. They do not inherit earlier speed or quality claims.

Initial engine A/B should preserve these exact quantized weights wherever
admission permits. Any required conversion must carry an explicit per-tensor
equivalence relationship; requantization or a different physical policy is a
separate experimental axis. Native source interpretation, tokenizer/conversation,
target/support pairing, complete artifact and binding admission, numerical
qualification and hosted execution remain prerequisites, not consequences of
having the files locally. Never relabel this pair with the existing source pin,
reuse a foreign runtime binding, or silently mix target and draft checkpoints.

## Target architecture

Layers 0 and 1 use sliding-window attention. Layers 2 through 42 alternate 21
compressed sparse-attention layers at ratio 4 with 20 heavy-compression layers
at ratio 128. SWA uses base RoPE without YaRN; compressed classes use the
versioned YaRN extension. HCA retains incomplete groups as raw local history
and composes raw and compressed representations without cross-representation
deduplication.

mHC represents four 4096-wide streams as one 16,384-wide residual state. Its
transitions carry 24 by 16,384 mixing geometry, 20 Sinkhorn iterations,
pre/post attention transforms, deferred feed-forward transforms, and final
collapse before RMS normalization.

Every target layer has one shared expert and 256 routed experts with top-6
selection. The first three layers use token-ID hash routing. The remaining 40
use a learned BF16 router, sqrt-softplus scores, correction bias,
deterministic no-aux top-k, and normalized route weights. Generic MoE,
runtime, and backend owners execute the family-selected schedule.

## DSpark architecture

DSpark proposes a block of five positions. The target execution plan captures
the four mHC streams after target layers 40, 41, and 42, averages each tap,
and projects the normalized features into three ordered draft stages. Draft
queries use noise token `128799` and mutually visible block positions.

The three draft Transformer/MoE stages are distinct from the public
configuration's `num_nextn_predict_layers = 1`. The bundled inference
configuration declares `n_mtp_layers = 3`, and the exact tensor geometry
establishes those three executable stages. Stage zero owns the main feature
projection and normalization. The final stage owns output normalization, the
rank-256 Markov projections, confidence projection, and the final hidden
combination. Target embedding and vocabulary output resources are shared by
identity rather than copied into a second model.

The Markov component is a low-rank token-conditioned vocabulary bias, not a
second persistent recurrent state. Confidence values are scheduling facts;
they never replace full-target verification or authorize publication.

## Tokenizer and output

The target has a 128,000-entry base tokenizer plus 1,283 added-token records
and an untied 129,280-entry output head. The tokenizer owner admits exact
prompt rendering, UTF-8 encode/decode, special/EOS classification,
incremental detokenization, and committed-token append semantics.

The source declares a one-million-token context contract. Hosted context
capacity is selected by the startup profile and admitted by the engine
specialization; source capacity is not an automatic runtime configuration or
performance claim.

These facts are sealed once by the family projection into model-execution
descriptor schema v1. The descriptor also carries RoPE/YaRN, attention-class,
compression, indexer, mHC, FFN, output, proposal, feature-source, Markov,
confidence, special-token and persistent-state geometry. Common runtime,
server, protocol, CLI and generic backend code consume that descriptor rather
than duplicate these values.

The pinned conversation encoder owns three admitted modes. Chat/non-think emits
the assistant marker followed by `</think>`; think-high emits the marker
followed by `<think>`; think-max additionally prepends the encoder's exact
maximum-effort instruction. These source facts live in the model-family
conversation descriptor. Common runtime, protocol and adapters consume typed
policies and channels and contain no DeepSeek token literals.

In thinking mode, output before the exact source-authored `</think>` token is
explicit reasoning and output after it is final content. The tokenizer owner
consumes the delimiter and refuses an unfinished reasoning grammar. It does not
search disabled-mode prose or classify delimiter-looking text in ordinary
final content. DSML tool blocks are parsed only through the source grammar and
remain a third typed result. This is model-emitted text, not access to hidden
internal chain of thought.

Ordinary multi-turn prompts apply the encoder's `drop_thinking` behavior:
reasoning from assistant turns before the latest user turn is omitted. A
tool-enabled prompt disables that drop so reasoning, calls and ordered tool
results retain the continuity required by the official format.

The independent `reference.deepseek.official-encoding` gate authenticates the
four upstream encoding/parsing cases and gold strings at the exact DSpark
revision above, using [`tests/vectors/manifest.json`](../../tests/vectors/manifest.json).
It executes the unmodified upstream test, compares native artifact-bound BPE
encoding/decoding for all four strings against `tokenizers==0.20.3`, and compares
the supported request prefix of case 2 against the upstream encoder. Thirteen
additional text controls and three simple prompt controls remain separate.
This is not native projection of every tool/developer/reminder transcript and
is not an official full-model logits oracle. Source code, gold files, tokenizer
data and their upstream MIT license stay in the immutable external source
snapshot; absent or changed backing fails the gate rather than downloading an
unpinned substitute. Run `make test-deepseek-official-vectors` with
`DEEPSEEK_SOURCE`, `DEEPSEEK_SELECTED_ARTIFACT`, `YVEX_RUNTIME_BINDING` and the
pinned test-only `YVEX_TOKENIZER_REFERENCE_PYTHON`. No GPU or generation runs.

## Coverage, transformation, and artifact

Exact source coverage reconciles 72,317 source tensors, 3,130 more than the
superseded snapshot inventory. The sealed Transformation IR has 1,409 terminal
descriptors: 1,328 target-trunk descriptors and 81 DSpark descriptors. Every
source tensor is required and mapped, explicitly shared, or rejected; scale
companions, expert coordinates, target taps, draft stages, and global shared
resources remain distinct.

The current bootstrap physical policy is
`deepseek-v4-flash-dspark-bootstrap-q2-v1`. It preserves the admitted mixed
IQ2_XXS/Q2_K decisions for the same target roles, not on an assumption that the
replacement checkpoint has identical payload bytes. The retained DS4
importance matrix remains bound to its predecessor source identity and is used
only as a bootstrap prior; it is not represented as DSpark calibration. New
draft norms, controls, feature and Markov projections, confidence tensors, and
other sensitive small roles use conservative exact, BF16, or Q8_0 storage.
Draft expert decisions are role-specific and cannot inherit an aggressive
low-precision default from their auxiliary scope.

The complete GGUF records source, logical model, transformation, physical
variant, target and draft role inventories, DSpark configuration, and exact
artifact identity. One runtime binding requires target execution, draft
execution, complete target verification, persistent target state, and bounded
draft workspace. Container validity or target-only opening alone does not
establish DSpark support.

## Hosted execution

One immutable engine generation owns both `target-only` and `dspark` execution
plans under its authenticated package and deployment specialization. One
engine-bound server session owns committed target state, token ledger,
transcript, incremental decoder and sampling state, plus bounded draft and
verification candidate state. No second process, model opening, tokenizer,
session registry, CUDA context, or output head is created for drafting.

In DSpark mode, proposals do not advance position, KV, transcript, usage, or
text. The complete target verifies the ordered candidate block and retains an
exact checkpoint after each target-authored row. Greedy mode
requires exact target-token equality; admitted stochastic mode uses
target-distribution-preserving accept/reject and residual sampling. The runtime
promotes only the accepted target-authored checkpoint without replaying
accepted rows, discards the rejected suffix, and publishes text after model,
token, decoder, and RNG state agree.

Target-only remains the explicit semantic reference and debug mode. An
explicit DSpark request fails closed when any draft tensor, plan, qtype,
workspace, backend capability, or policy requirement is absent. It never
silently falls back to target-only.

For explicit reasoning, the source-authored terminator ends the speculative
shape; the final channel continues with ordinary target decode. The
`source-boundary` event reports boundary extent in `a`, the committed extent
after that boundary in `b`, and replayed accepted target rows in `c` (required
zero). A speculative block crossing the terminator can also commit final-channel
tokens; `b` and final-channel token counts are therefore not exact counters of
ordinary target-only work. Subsequent iterations use ordinary target decode.
This identity-bound sub-policy is not silent fallback.

## Current capability

DeepSeek-V4-Flash-DSpark has a complete YVEX source-to-streamed-text path
and is the only currently admitted target-verified speculative family.
The hosted native, interactive, and bounded OpenAI-compatible paths
consume one target-verified runtime authority. Target-only and DSpark modes,
multi-turn reuse, cancellation, reset, and committed-only streaming are
implemented under the [current local protocol](../contracts/local-protocol.md). The admitted tokenizer/prompt
profile also supports explicit reasoning high, maximum, and disabled policies
through separate reasoning, final, tool, and error channels.

## Explicit non-claims

This record does not claim:

- an optimized GB10 physical variant or a DSpark speedup;
- native MXFP4/NVFP4 Tensor Core execution;
- production load-aware confidence scheduling or continuous batching;
- complete accelerator residency or an entirely device-side generation path;
  admitted CUDA greedy/stochastic selection and speculative correction retain
  common transactional RNG and host tokenizer ownership;
- multi-device or distributed serving;
- model behavior or quality evaluation or quality parity;
- a public full-model benchmark;
- speculative support for another family;
- release qualification.

Current gate state is owned only by [Status](../project-control/STATUS.md).
