<!-- docs:metadata
title: Liquid LFM2.5 native qualification
id: yvex.model-families.lfm2
document: reference
status: mixed
owner: model
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Liquid LFM2.5 native qualification

[Model families](README.md) · [Program P](../research/physical-model-compiler.md)

**Current boundary:** complete hybrid/MoE program, lossless BF16/F32 parameter
recipe, independently verified full GGUF and reimported runtime bindings.
A three-block scaled program agrees exactly with a pinned external CPU
reference. CUDA convolution, normalization, eager-attention and routed-expert
consumers compile but have not been dispatched. These are not whole-checkpoint
inference results. No Liquid Engine, complete-checkpoint generation, performance,
held-out quality or YAI
acceptance is established.

## Checkpoint and representation

| Fact | Authority / value |
| --- | --- |
| Official model | `LiquidAI/LFM2.5-24B-A2B` |
| Original source revision | `1769516ac588c4c558089c94f27d96b0dc67b183` |
| Architecture | `Lfm2MoeForCausalLM` / `lfm2_moe` |
| Acquired parameters | Ten Safetensors shards; 7,610 tensors; 47,943,180,288 tensor bytes |
| Source precision | 7,572 BF16 tensors; 38 F32 router-bias vectors |
| Native payload identity | `3dcd02062afc89181c88e308d16d68ec421271ef57f47b7ba7199ebeb99e02d7` |
| Reference implementation | Transformers v5.12.1, `ddb849abe009d1089e6c691bfc897f27211c663c` |
| Model license | Source-retained LFM Open License v1.0; not the YVEX MIT code license |
| Official GGUF reference | `LiquidAI/LFM2.5-24B-A2B-GGUF`, observed revision `0912d33141f77e10705345a6732bfd83f0252fdc` |
| YVEX artifact | `ffd820b1614f994c4e146c49f5e29278029623ef86a90f67efee95f7bcd7612c` |
| Produced CPU realization binding | `4bce9e08d9f641befcf40b162589c39251245797aeb2e73947dd35138b5fd159` |
| Compiled CPU/CUDA candidate binding | `29b412a3ac72019b0c1e657e79b23a4514fb36153e4fb665645b6447682a4df2`; no CUDA execution claim |

The [official GGUF record](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B-GGUF)
identifies **QAD-Q4_0 as a separately quantization-aware-distilled checkpoint**.
It cannot provide same-checkpoint quality evidence for an ordinary quantization
of the original BF16 source. A QAD-versus-original comparison changes both
representation and checkpoint; it is not an isolated engine comparison.

### Exact tokenizer execution

The produced artifact retains the original tokenizer JSON and its authenticated
125,017-ID domain. `llama-bpe` now executes its actual regex alternatives,
including letter/mark distinctions, contractions and newline-terminated
whitespace. The embedded `ignore_merges` boolean permits whole-piece vocabulary
lookup. Otherwise merges apply one leftmost minimum-rank pair at a time:
merging every occurrence simultaneously is incorrect for this source's
non-topological merge ranks.

The independent `tokenizers` 0.22.2 producer in
[`tests/reference/lfm2_tokenizer.py`](../../tests/reference/lfm2_tokenizer.py)
checks 128 public vectors against the actual GGUF and binding: multilingual and
control bytes, added tokens, repeated newlines and deterministic mixed text.
Initial execution split 17 newlines into IDs `[2,1]` instead of `[0,3]`; after
the correction the expanded vector set agrees exactly on IDs and decoded bytes.
Separate empty-decode, literal-control and malformed-option checks cover identity
and refusal. Missing BPE options retain their false default, including previously
admitted minimal fixture metadata; malformed or duplicate options still refuse.
This does not establish chat-template, native-tool or full-model qualification.
Unicode letter tables are reproducible from digest-pinned Unicode 15.0.0 data
with `tools/generate_tokenizer_unicode.py`; existing notices apply.

The acquired source also includes `chat_template.jinja`, digest
`ea663864491de7ade391839479860ca95541f892f72665c73251fbd4643b1bef`.
The current binding deliberately admits `verbatim-lfm2-v1`, not that template's
reasoning and Python-like native-call grammar. Raw tokenizer-aware continuation
and source-faithful conversation adaptation are separate qualification gates.

The native source catalog authenticates the pinned index; the
[family interpreter](../../src/model/families/lfm2.c) checks the exact repository
and revision, configuration, role population, shapes and per-role dtype.
Metadata/header verification remains distinct from payload-trusted manifest v3.
The native `source verify` command independently streamed all ten shards and
published `upstream_payload_verified`: every observed SHA-256 matched its
Hugging Face Git LFS authority. Acquisition hashes did not silently promote
the native manifest. The retained verification copy is separate from the
original acquisition records.

## Source computation

| Geometry | Source-defined meaning |
| --- | --- |
| 40 blocks | 30 double-gated short convolutions; ten full-attention blocks |
| `layer_types` | Owns the topology; the importer does not infer it from layer-number periodicity |
| Hidden width / convolution | 2,048 channels; depthwise causal kernel 3, no bias or activation |
| Attention | 32 query heads, eight KV heads, head width 64; Q/K RMS normalization and half-split RoPE |
| Feed-forward | First two blocks dense, width 11,776; remaining 38 blocks sparse MoE |
| Experts | 64 per sparse block, top four, expert width 1,536 |
| Routing | Sigmoid logits; F32 expert bias affects selection, not the selected mixture weights; normalization adds `1e-6` |
| Tied embedding/output | 128,000 parameter rows; no separate `lm_head` tensor |
| Tokenizer | 125,017 published IDs; the remaining embedding rows do not manufacture token strings |
| Context | Source maximum 131,072; no YVEX execution capacity claim |

The convolution consumes contiguous projected groups `B, C, X`:

1. Publish `B * X` in BF16.
2. Apply a depthwise causal convolution using the source tap order.
3. Publish convolution output in BF16 and multiply by `C`, again publishing BF16.
4. Apply the independent output projection.

Its cache retains the most recent three `B * X` values per channel, oldest
first. It has no recurrent SSM matrix, DeltaNet decay, convolution SiLU, attention
gate or fabricated KV state. Residual additions, normalization, attention,
MoE and output projection remain separate mathematical obligations.

## Implemented compiler-to-state path

[`sequence.double_gated_conv`](../../src/ir/sequence.c) takes BF16 projected
rows, a BF16 `[width, 1, kernel]` parameter and an F32-carried
`convolution.causal` state. It returns BF16 rows and a typed successor state.
The verifier checks dimensions, precision, state domains and declared read/write
effects. Stale-state returns and incompatible parameter encodings refuse.

The common [physical lowering](../../src/graph/program_physical.c) derives sealed
geometry and a convolution-only state binding. The initial
`double_gated_conv.cpu.bf16.v1` remains CPU-only; the new
`double_gated_conv.bf16.v2` carries the same arithmetic to an explicit device
consumer as well. Compilation is not device qualification.
Binding schema v2 denotes this state class; existing v1
bindings and their identities retain the mandatory recurrent population.
The plan container remains v1 and validates each binding's schema.

The [runtime consumer](../../src/runtime/program_sequence.c) resolves explicit
state roots and prepared parameters. The existing sequence-state owner allocates
two banks, stages a successor and commits or aborts with the session transaction.
For width 2,048/kernel 3 each bank is 24,576 bytes. Parameters, activations and
the rest of the model are additional costs; these bank counts are not a model
admission estimate. CPU execution needs no convolution scratch. CUDA uses a
four-byte status word allocated and accounted by program-sequence preparation.

The CPU realization uses ordered F32 fused multiply-add per tap, BF16
round-to-nearest-even publications and finite, BF16-exact operand checks.
It refuses overlapping input/output/state buffers, insufficient capacities,
stale geometry and nonfinite operands/results. Cancellation or numeric refusal
does not publish candidate state. The CUDA implementation assigns independent
channels to threads, preserves tap ordering and checks completion in at most
64-row chunks. Its compiled, undispatched controls cover invalid operands,
overlapping scratch, cancellation, continuation and transactional abort.
Metal remains unsupported.

CUDA routed SwiGLU consumes resident source BF16 banks without copying or
preparing another complete weight population. Its at-most-eight-row tile uses
`64 + min(rows,8) * (2*experts + selected + selected*intermediate)` F32 carrier
values, including aligned status storage. The exact source geometry at four
selected experts uses 201,088 bytes for eight rows. The backend query owns this
scratch geometry; common program preparation allocates, accounts and releases
it. It is not total model memory. Ascending-expert accumulation, BF16 publications
and cutoff-tie refusal remain explicit. Eager attention uses a separate bounded
score tile and the existing transactional K/V owner.

The registered `reference.lfm2.program.cuda` runs the same pinned scaled
program as the CPU reference, including chunk/single, cancellation, retry and
allocation cleanup. `cuda.program` adds routed invalid-input/weight/tie and
scratch-reuse controls. These are prepared accelerator gates, **not executed
evidence**. Whole-checkpoint admission and generation remain separate.

## Complete parameter and program construction

The family producer compiles embedding, rounded-input RMS, nongated GQA with
half-split rotary, dense SwiGLU, biased-sigmoid routed SwiGLU, residuals, final
RMS and tied output. Explicit state roots distinguish ten K/V populations from
thirty convolution histories. No recurrent SSM matrices or attention gates are
manufactured.

The complete recipe maps 7,610 source tensors to 428 terminals: 390 BF16 and
38 F32 bias vectors. Ordered axis-zero `STACK` gathers each expert bank.
Expert-major addressing counts rows across the expert axis. Scalar bits are
preserved; this is not quantization or QAD. The tokenizer reader supports the
source's pair-array merges without inventing absent BOS/EOS insertion settings.

| Produced fact | Retained value |
| --- | --- |
| GGUF bytes | 47,969,264,224 |
| Parameter bytes checked against source | 47,943,180,288 across all 428 terminals |
| Transformation | `a161c69911d1e270bd0c53f85de6fb773367ec186f352d2d0b8fb82e3dbfe6c8` |
| Physical policy | `fe9a0e69e792e64e1dead0f5dd23f732019f79279e2ff2389693f2e5ea7ca581` |
| Verification | Native roundtrip and independent GGUF reader accepted |
| Production observation | 254.32 seconds; process RSS about 304 MiB; bounded cgroup peak about 1.077 GiB |
| Binding preparation observation | 45.35 seconds; maximum process RSS 150,272 KiB; no Engine created |

The first binding describes the CPU realization before convolution lowering v2.
The new binding above seals the changed executable program. Its forward program
is `62cd22f8ab8d4f4444679d46146463c216d00d73eb5328340b70ec5bc0333bc4`;
the source graph and parameter identities are unchanged. Preparation took
48.87 seconds with 150,388 KiB maximum process RSS and no Engine creation.
Both bindings remain intact. The immutable GGUF is reused without copying weights.

The `compile optimize` CPU readbacks of both bindings resolve the exact block/expert population,
834 forward operations and three tied-output operations. Resource admission
correctly refuses `fixed runtime resources exceed usable memory` inside its
deliberately bounded 2-GiB verification cgroup. This is not a measurement of
exclusive GB10 model capacity.

Private production/readback records, including separate executable/build/delta
identities, are under
`/home/dgmothx/lab/models/evidence/lfm2-production-20261020.FCtOqX1v`.
No weights, private vectors or build products are committed.
The [compiler evidence](../evaluation/compiler-evidence.md#liquid-source-to-binding-continuation-2026-10-20)
records the source-stable CPU qualification and its explicit remaining gates.

## Integrated CPU numerical reference

[`tests/reference/lfm2_program.py`](../../tests/reference/lfm2_program.py)
executes unmodified bodies from pinned Transformers v5.12.1 with public
deterministic weights: three blocks (convolution/attention/convolution),
hidden width 16 and four experts/top two. It is independent computation.

An initial discrepancy exposed a missing numerical distinction: eager attention
publishes QK contraction, scaled scores, normalized probabilities and output in
BF16. The former F32 score/probability path is not equivalent. The explicit
`BF16_EAGER_V1` class corrects those boundaries rather than relaxing tolerance.

| Control | Before | Corrected observation | Claim |
| --- | --- | --- | --- |
| Hidden state | 45/96 unequal; max absolute difference 0.01171875 | 96/96 exactly equal | Scaled CPU program |
| Logits | 79/192 unequal; max absolute difference 0.00390625 | 192/192 exactly equal | Scaled CPU tied head |
| Three greedy steps | Not promoted | Exact reference continuation | Bounded synthetic continuation |
| State lifecycle | Separate control | Chunk/single equality; cancellation, abort and retry pass | CPU transactional composition |

Failure receipt:
`47bf7fb788ec8b19f32aa2260929d3e860e4993db8158bd529f645f95e0ea39e`.
Corrected receipt:
`cfdd1779ba58f04d8a71757b12670ac92f631d0f49cc74ea090dbce939d39bf7`.
The corrected receipt belongs to the CPU binding-producing snapshot, not a
later CUDA candidate. Full-checkpoint logits, NLL/PPL, held-out quality and
performance remain unavailable.

## Numerical and lifecycle evidence

| Control | Scope / boundary |
| --- | --- |
| `unit.lfm2_source` | Scaled complete role inventory and configuration/shape/dtype refusals |
| `live.lfm2.source` | Actual pinned source, all 7,610 tensor roles and exact byte accounting; does not produce a model artifact |
| `unit.short_conv` | Typed compilation/import, independent dyadic F64 oracle, chunk/single continuity, commit/abort/fork, exact state costing and device refusal |
| `reference.lfm2.short-conv` | Nine real first-layer projected inputs, real convolution weights, pinned external PyTorch uncached convolution output and final state |

The external fixture producer is
[`tests/reference/lfm2_short_conv.py`](../../tests/reference/lfm2_short_conv.py).
It verifies the upstream implementation SHA-256 before evaluating its unmodified
class, reads only bounded CPU source tensors, and retains weights/vectors outside
Git. The native comparison requires exact equality for all 18,432 outputs and
6,144 state values; token-by-token execution must preserve the same result.
These observations concern the convolution boundary, not all-layer logits.

The pinned Transformers **cached slow path rounds each BF16 product before its
sum**, whereas its uncached convolution and accelerated path have different
realizations. The fixture explicitly records that difference. Exact agreement
with uncached Conv1d is not a certificate for every upstream slow/fast path.
Whole-model numerical policy must resolve this distinction before model admission;
no tolerance or checkpoint-quality claim is inherited.

### Retained qualification — 2026-10-20

The qualified software snapshot is main `ecd9f44314150f5f110337c408cc3f5b1f5fcf58`,
tree `190b410e627fb9d872b3cd72c6a7a485a805dd5b`, with exact dirty delta
`da63c0e93c40b3e3b41aa13316694df0479aa1149bda8ac4e6910c18c1ba195a`.
It includes retained concurrent Program P changes; it is not a clean-source
release certificate. Native build identity is
`e756b9dfa05794fe0aa2cd2b60a58e5789211d11d6d4f1219a4b5ab951de9fab`.
Receipts below are canonical local QA evidence under `build/qa/evidence/`,
not published full-model qualification.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fast` | Registered CPU controls | DeepSeek, Qwen, Mamba and new Liquid fixtures | All controls pass | 107 PASS, zero FAIL/BLOCKED | Exact assertions | PASS | Software regressions |
| `structural` | Ownership, ABI and publication rules | Current repository | No structural violations | 21 PASS, zero FAIL/BLOCKED | Exact rules | PASS | Repository structure |
| `sanitizer.runtime` | ASan/LSan and UBSan | Runtime fixtures including Liquid source/convolution | No sanitizer failures | Both executions pass; one registered lane PASS | No accepted sanitizer findings | PASS | Bounded memory, lifetime and refusal controls |
| `live.lfm2.source` | Pinned source and native v3 manifest | All acquired source headers | Complete legal role coverage | 7,610 unique roles; 47,943,180,288 tensor bytes | Exact geometry/dtype/count | PASS | Source interpretation, not inference |
| `reference.lfm2.short-conv` | Pinned external class | Real layer-zero weights, nine public token IDs | Equal output/state and single-token continuity | 18,432 equal outputs; 6,144 exact state values | Maximum absolute difference zero | PASS | Component numerics, not model quality |

Receipt identities, respectively:

- `fast`: `916f9cbe28f6715f906988fd9af7a3ca562965870ac54033400a8957bdc44ab0`
- `structural`: `c972bb5427d13f629de32e9af419d0f3cc66dc5b1ec50af50431aa73626ad3be`
- `sanitizer.runtime`: `c54ec2973355c06e2b840f1fa798186819e9761061ece0a73401dbe0230a5142`
- `live.lfm2.source`: `5637ffe3cd2fa2d1a45abf414a5b5cadad53b30b1864987250796d5e702df5c1`
- `reference.lfm2.short-conv`: `a8a9be93adb6ae1f7f93c5eded98ac4b622f48a63ebc5b9a5be0cead2130ce10`

The independently regenerated private fixture has SHA-256
`2a6c89c9f6bde70c54c618b0c77c6824282716bce3b19293a379d74f023ba4ea`.
The uncached versus cached-slow upstream comparison differs at 4,695 of
18,432 values, maximum absolute difference `0.001953125`; that negative
control remains part of the numerical-class decision, not an accepted
whole-model tolerance.

The initial catalog addition exposed a positional default-target dependency:
the release accessor now resolves its existing explicit target ID. The
DeepSeek release identity is unchanged and the regression has a dedicated
assertion. Earlier failing receipts remain retained; they are not passes.

The subsequent scalar-aggregation control adds an independent parameter-production
boundary: `unit.quant_execute` receipt
`12177950497ea6a4807ea0678fa4535eea01ee28f5c3d1d688933ae23a474bfb`
passes on dirty delta
`d3d78348c4e7cfb2ada327dbc04ad281a99da262798d4f009b23e4b5f23173c7`.
It checks actual lowering, ordered exact BF16 bytes, whole-terminal abort,
cancellation/reuse and wrong-axis refusal. A source stream has a local byte
offset while the terminal has a cumulative element offset; the executor now
keeps that distinction for successive operands. This test does not produce
the complete Liquid artifact or explain the unrelated r17 numerical failure.

Final software revalidation, including scalar aggregation, uses the same
base commit/tree and dirty delta
`0e740ed1c86a8cd427509920f1f7355b9d68a03979bc6424e961b4e9ba7ce834`.
Native build is `00e1249576b421f1e37a9010129040d79707a76b4b571838268a7c8a17cfd9b3`;
the local product executable SHA-256 is
`49f27769a72af48fb49d10bd56abcbf6e65eba62fe5f4659e0dc72185d37d644`.
It is not the installed service executable. All five receipts retain
`source_stability.valid=true`:

| Final gate | Registered result | Receipt |
| --- | --- | --- |
| `fast` | 107 PASS, zero FAIL/BLOCKED | `3a39ece5fe2c6d1e2f73a6119591b55fc11b0e75763752c02692f7d0f41f6d65` |
| `structural` | 21 PASS, zero FAIL/BLOCKED | `e27339e00686e46d95d7b54b4fa83b128b0874572ea82e24a9b3f39431d86058` |
| `sanitizer.quant` | ASan/LSan and UBSan pass; one registered gate PASS | `e2db676dd98a8e153b7891e5d4d08c34d14c1328cc68116591c603616129f150` |
| `live.lfm2.source` | 1 PASS | `8801fdf4c9c3e257ad335fd9e855e0b2ef724effddaff8e9aed6b50b0850579c` |
| `reference.lfm2.short-conv` | 1 PASS; exact outputs/state retained | `5f06b2fbdc2ad9eaac200884c213b7bb3d1dcacc9c505f9ff70d9d92d6056d23` |

Only the evidence documentation changes after that software snapshot. These
registered results do not assert native macOS, CUDA model execution, a
checkpoint-quality gate or a throughput measurement.

Private raw evidence is retained under
`/home/dgmothx/lab/models/evidence/lfm2-native-20261020.Et9I6kxu`.
Reproduction uses `tests/reference/lfm2_short_conv.py` with the pinned upstream
file, then `YVEX_LFM2_CONV_ORACLE=<fixture> python3 tools/qa.py run
reference.lfm2.short-conv`. Source auditing uses `YVEX_LFM2_SOURCE`,
`YVEX_LFM2_MODELS_ROOT` and `YVEX_LFM2_MANIFEST` with
`python3 tools/qa.py run live.lfm2.source`. These checks do not dispatch GPU work.

## Open production and execution gates

| Owner | Required next result |
| --- | --- |
| Family / typed program | Whole-checkpoint numerical validation of the implemented hybrid program; scaled CPU composition does not suffice |
| Parameter compiler | Full lossless recipe produced; quantized alternatives need separate lineage and quality evidence |
| Artifact / binding | Verified artifact and CPU binding retained; changed executable programs need their own binding and tokenizer-aware generation validation |
| Backend / deployment | Qualify implemented CUDA convolution/normalization/eager attention/routed SwiGLU, then independently admit full workspace/state/preparation costs |
| Evaluation | Independent layer/logit/held-out quality controls and actual native generation |
| GB10 comparison | Fresh operator authority, full capacity admission and uncontended same-machine llama.cpp/YVEX workloads |

The installed 0731 is protected. A small Liquid file does not prove safe
co-residency. No model unload, service change or accelerator experiment follows
from this source/CPU work. Liquid qualification does not repair DeepSeek r17 or
earn Program P's DeepSeek 20/700 performance exits.
