<!-- docs:metadata
title: Run the MiniMax Media Path
id: yvex.guides.multimodal
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Run the MiniMax Media Path

**Operate the bounded current composite model path.**

[Up](README.md)

## Direct MiniMax-H3 media host

MiniMax-H3 uses the same persistent server and local chat protocol, but its four
large component artifacts are staged at request phase boundaries rather than
kept resident simultaneously. Its installed composite startup profile owns the
component location, CUDA backend, and media mode. Normal operation is therefore
the same registry-first command used by other hosted models:

```sh
./yvex model show minimax-h3-fl2va
./yvex serve
./yvex model load minimax-h3-fl2va
```

The default publication directory is `$YVEX_DATA_DIR/media`, or
`$HOME/.local/share/yvex/media` when that override is absent. YVEX creates and
admits it as an owned absolute non-symlink directory. `--output-root` and
`--media-artifact-root` remain explicit engineering overrides; they are not
normal startup requirements. Startup opens the tokenizer and all four
identity-bound component artifacts. The server retains their admitted immutable
views under one engine-generation identity, but does not preload the component
payloads or create a CUDA context. A completed media request stages each
already-admitted component through the native YVEX runtime. The composite
registry profile is a local deployment contract; family semantics still select
and validate the exact four-component topology.

Component byte authentication uses the common verified-reopen authority. A
cold open fully verifies a component and publishes its snapshot-bound receipt;
an unchanged warm open authenticates that component without rereading its full
payload. Missing, malformed, stale, or unusable receipt evidence falls back to
full verification and is repaired only after the bytes match. The four
components are independent: one fallback does not invalidate three valid warm
reopens. This mechanism does not materialize weights or imply host/CUDA
residency.

From another terminal, start the ordinary client:

```sh
./yvex chat --model minimax-h3-fl2va --session video
```

Submit one creative prompt. The prompt immediately starts native generation;
there is no parameter questionnaire, keyword parser, assistant model, or
conversation planner. Prompt bytes are opaque execution input and reach the
existing tokenizer/conditioning path unchanged by operator policy.

Ordinary hosted execution selects the identity-bearing released FL2VA policy:
50 sigma points, 49 paired evaluations, terminal zero, a 1344x768 default
canvas, 124 frames, and seed 42. Select the released canvas, duration, and
deterministic seed when entering the native client, then type the creative
prompt at its prompt:

```sh
./yvex chat --model minimax-h3-fl2va \
  --trajectory released --width 768 --height 768 \
  --duration 5 --seed 42
```

Dimensions are paired, multiples of 32, within the released area and aspect
envelope, and include square, wide 1344x768, and portrait 768x1344 canvases.
Duration is aligned upward to the released `17n+5` frame rule and must remain
inside the 124-to-345-frame contract. The terminal result reports the resolved
duration, frame count, evaluation count, seed, trajectory, RNG, plan, engine,
and publication identities. `--trajectory preview` retains the separate
`interactive-preview-v1` YVEX test policy at 192x192, 124 frames, one
evaluation, and seed 42. Creative words such as `HD`, `seed`, `draft`, `MOV`,
numbers, or durations never alter either typed policy.

Chat projects server-authored conditioning, latent, decoder, publication,
completion, cancellation, and failure events as control state rather than
model-authored prose. On success it renders the typed publication path and
media identities. Ctrl-C uses the existing request cancellation contract and
must leave no partial published file.

The released maximum wide dual-anchor plan contains 106,238 packed rows. The
generic CUDA joint Transformer admits up to 131,072 rows and uses a 64-query
chunked exact attention path whose maximum released workspace is
15,230,279,684 bytes. The family admits 16 GiB of workspace and 64 GiB of peak
device resources, while retaining explicit resource refusal rather than
silently reducing canvas or trajectory. Preview names still describe bounded
geometry, not model quality. The smoke profile retains the earlier repeatable
32x32 evidence. The historical
pre-direct-execution server/chat acceptance used `smoke`, five seconds, two
sigma-grid points, AVI, and seed 42; it returned a 1,048,544-byte seekable file
after 560.36 seconds.
Independent GStreamer playback recovered 124 frames and 165,333 stereo samples
per channel with a 10,416 ns duration delta. Peak server RSS was 62.57 GiB
inside the 88 GiB hard limit, with no residual component residency after the
turn.
