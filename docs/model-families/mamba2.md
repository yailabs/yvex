# Mamba2 pure-SSM compiler boundary

Status: PARTIAL / exact artifact-executable. Source authority, deterministic
artifact and binding admission, the common CPU compiler/runtime path and exact
all-layer/output/session execution are qualified. Independent all-layer or
whole-model numerical conformance and hosted text generation are not claimed.

The reference is `mistralai/Mamba-Codestral-7B-v0.1`, acquired through
`yvex model pull` at immutable Hugging Face revision
`4f086c08c1e0f07bdc50ca25125dbbf7475d21da`. The selected
`safetensors-source` representation contains three numbered shards and nine
sidecars: 14,574,191,162 acquired bytes, including 14,570,807,296 tensor bytes.
The alternative consolidated payload is not part of this acquisition.

Source verification authenticates the index, provider hashes, headers and
exact tensor population. It admits 579 BF16 tensors: three global roles and
nine roles per layer across 64 layers. Incomplete or duplicate shard
populations, Mamba1 declarations, hybrid/attention topology and inconsistent
group/head/state geometry fail closed. The acquired geometry is hidden 4096,
intermediate 8192, vocabulary 32768, 128 heads of width 64, state dimension
128, eight groups and convolution kernel four. These are source-derived facts,
not runtime switches.

The durable catalog source manifest now binds the canonical provider location,
repository and immutable revision directly. Its aggregate manifest identity is
`67f63fd2d04e240aaf09d436e2fe2eec6c086a28e4e78f2829cc303943b8a75e`.
Source verification precedes artifact construction; stale paths, revisions,
aggregate identities, representations and tensor populations remain refused.

## Source-owned execution policy

[The architecture importer](../../src/model/families/mamba2.c) preserves raw
metadata conflicts while resolving only the facts justified by the exact
source and its authoritative model implementation:

- tokenizer vocabulary and tokenizer configuration own token identity: UNK 0,
  BOS 1, EOS 2 and no PAD token;
- generation policy admits BOS insertion and EOS termination only when the
  declared generation EOS agrees with tokenizer identity;
- the conflicting `config.json` BOS/EOS/PAD values do not override tokenizer
  identity, and an impossible PAD declaration is not manufactured into a
  usable token;
- the exact source has no admitted chat template; absent or explicit-null
  template metadata means no template, while a non-null unowned template is
  rejected;
- the examined official Mistral implementation at revision
  `9eaeb91c17450e09021b6065a1d5cc69876507c8` selects state-spaces Mamba2 with
  the source `n_groups`; that recipe establishes gate-before-normalization with
  grouped RMS reduction for this target. The conflicting
  `norm_before_gate=true` source field remains recorded rather than silently
  becoming execution policy.

Tokenizer truth and generation output policy are separate compiler facts.
Neither frontend convenience nor a backend is allowed to adjudicate them.
Malformed special-token declarations, conflicting generation EOS, a declared
PAD not present in the vocabulary and unsupported normalization/gating policy
fail during import.

## Compiler and runtime ownership

The importer projects the exact source into a typed pure-SSM module. Its
`forward` entry takes tokens, positions and explicit convolution/recurrent
state values, then returns hidden values and successor state versions. Its
`output` entry applies the source-owned final normalization and LM-head
projection. The program contains token embedding, repeated Mamba2/SSD blocks,
final normalization and output projection without invented attention, KV,
RoPE or dense-FFN roles.

[`sequence.selective_ssd`](../../src/ir/sequence.c) owns the typed operation,
geometry verifier, state effects and successor-state contract. Common physical
lowering selects the current portable `selective_ssd.cpu.f32state.v1`
implementation. Common physical SSA execution, parameter bindings and
[sequence state](../../src/runtime/sequence_state.c) own invocation and
transactional state. There is no Mamba-specific session, allocator, decoder
loop, runtime resource ledger or KV cache.

The exact acquired program compiles all 64 layers and 579 parameters into 900
forward instructions plus two output instructions with 64 state bindings. The
source-faithful GGUF is 14,574,491,136 bytes and has artifact identity
`bf0053bf02a235563342a0281acfc73e17eb287542a7109e975db414458a77d3`.
Runtime binding v17 authenticates that artifact and the attention-absent
program under identity
`f8eae71cdef043cea3fc0b46264026f65944892c6b1703c7761c3eab61afabb6`.
The artifact reopens through normal admission and the engine consumes the
compiler-owned physical program; it does not reconstruct Mamba topology or
tensor roles during warm execution.

## Numerical and lifecycle evidence

[The reference helper](../../tests/reference/selective_ssd.py) compares the
first acquired layer with the pinned Mistral/Mamba-SSM semantics. With real
BF16 source tensors expanded to F32, 16,384 output values agree at maximum
absolute error `2.86102295e-6`; 40,960 convolution-state values agree exactly;
and 1,048,576 recurrent-state values have maximum absolute error
`5.7220459e-6`. The registered acceptance is absolute `3e-6` plus relative
`3e-5`. The larger state absolute difference remains within the relative term.
This is a first-layer component oracle, not complete-block or all-layer
conformance.

A bounded two-layer physical-program fixture proves exact chunk-versus-single
continuation for hidden and state values, output-head execution, candidate
state abort after cancellation, retry and commit. Exact artifact execution
then runs all 64 layers in compiler order, emits 32,768 finite logits and
repeats a fixed token after reset with the same logits digest
`b744e34360e40ed8ead0f3d8bbe5d35d71b327369fdbbba3f9ecaa2123447aef`.
Continuation advances retained state from position one to two; a cancelled
candidate remains unpublished, retry commits, reset returns to position zero,
an isolated session starts from the same initial generation and a stale
position fails closed. These are exact-artifact internal execution and
determinism results, not an independent all-layer numerical oracle.

Per layer, source geometry requires 40,960 convolution and 1,048,576 recurrent
F32 values. Across 64 layers the exact derived bank size is 278,921,216 bytes,
with one committed and one candidate bank. Runtime session summaries observe
those same two state allocations. The engine maps 14,574,491,136 artifact
bytes and reports zero copied host-resident weight bytes for the mapped CPU
path. Geometry, mapped package bytes, state allocations, workspace and process
overhead remain distinct resource facts.

## Remaining evidence boundary

The exact engine loads through the normal lifecycle on the CPU backend, but the
public/native hosted surfaces are conversational and the pinned source owns no
chat template. A bounded chat request therefore returns the typed unsupported
error `model has no admitted conversation template`; YVEX does not invent a
template, PAD token or frontend override merely to produce a demo. Token-level
execution through the common runtime is qualified, while hosted text generation
is not.

The pinned low-level tokenizer independently agrees on the retained vector
`Hello world` → `[23325, 2294]`; the admitted YVEX policy prepends BOS 1 and
decodes the same text, with EOS 2, UNK 0 and no PAD. The exact artifact also
retains gate-before-normalization with grouped RMS reduction. Token-policy,
normalization or artifact/binding identity drift is rejected at its owning
boundary.

Missing independent all-layer/whole-model and upstream evidence remains
BLOCKED or NOT RUN, never inferred from component or internal determinism.
A01 therefore remains PARTIAL even though its exact artifact/model execution
qualification is complete.

Public ABI 0.1.0, local protocol 22, OpenAI profile
`yvex.openai.compat.v3`, model-plan v8, PEIR v5 and Semantic Model IR v2 are
unchanged. Runtime binding advances internally to v17 because an authenticated
absence of attention is a different semantic contract from the v16 layout;
v14-v16 remain import-only compatibility forms. This work does not implement
Program N, chunk-parallel SSD, CUDA SSM, behavior evaluation, performance
qualification or release support.
