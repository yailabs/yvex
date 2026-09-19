# Mamba2 pure-SSM compiler boundary

Status: PARTIAL; source authority and the common CPU compiler/runtime boundary
are qualified, but no complete artifact, executable binding, loaded engine or
hosted generation is claimed.

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
forward instructions plus two output instructions with 64 state bindings.
This proves compiler-language and physical-program coverage. It does **not**
prove that a complete package/binding can be opened by an engine or that all
layers reproduce an independent whole-model oracle.

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
state abort after cancellation, retry and commit. Common state tests preserve
session isolation, reset, cleanup and stale transition/state refusal. Single
token continuation consumes retained state and does not replay the prefix.

Per layer, source geometry requires 40,960 convolution and 1,048,576 recurrent
F32 values. Across 64 layers this derives 10 MiB convolution plus 256 MiB
recurrence per committed bank, with an equal candidate bank. These are derived
geometry, **not measured hosted allocations**. Workspace, weights, persistent
state and candidate state remain distinct resource facts.

## Remaining promotion boundary

The repair does not publish a complete artifact, runtime binding, deployment
profile, loaded Mamba engine or hosted text result. The catalog's durable
source-manifest association also requires rebinding to the verified canonical
source location before artifact production; exact-source qualification used a
temporary manifest and did not rewrite acquisition authority.

The next evidence boundary must join the compiler-owned program to a complete
artifact/binding, retain source/executable/state identities, execute the exact
all-layer and LM-head path, and qualify prompt continuation and hosted bounded
generation under the explicit tokenizer/output policy. Missing independent
whole-model and upstream evidence remains BLOCKED or NOT RUN, never inferred
from the component result. A01 therefore remains PARTIAL.

No public ABI or wire protocol changed. The internal adapter, Semantic Model
IR and physical-program schemas use their existing version owners. This work
does not implement Program N, chunk-parallel SSD, CUDA SSM, behavior evaluation,
performance qualification or release support.
