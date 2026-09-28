<!-- docs:metadata
title: Recover Local Operations
id: yvex.guides.recovery
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Recover Local Operations

**Inspect the owning state before retrying or removing local resources.**

[Up](README.md)

## Local paths

- `$XDG_RUNTIME_DIR/yvex/yvexd.sock` is the private mode-0600 local protocol
  endpoint; its directory and singleton lock are private to the owning UID.
- `~/.local/share/yvex/models.local.json` stores local model registry entries,
  including complete startup profiles. `YVEX_DATA_DIR` may override the parent
  for controlled deployments. The file is local configuration, not tracked
  repository data.
- `$XDG_STATE_HOME/yvex/` is reserved for explicit opt-in history, log, and
  trace sinks. The current client does not persist prompts, answers, tokens, or
  sequence state.

When XDG variables are absent, the client uses the documented HOME-based
configuration fallback and the protocol owner uses its private runtime
fallback.


## Recovery

- Missing socket: run `yvex serve` and wait for `host status` to report the
  host ready, then use `yvex model load`.
- Stale or unsafe socket: verify UID, mode, runtime-directory ownership, and
  singleton-lock ownership; never delete another user's socket.
- Binding or artifact mismatch: select the binding for that exact artifact
  identity; never bypass admission.
- Partial session: inspect it, then explicitly reset or close it before an
  ordinary new turn.
- Unsupported CUDA: start an admitted CPU host or repair CUDA admission; no
  CUDA request falls back silently.
- Queue refusal: wait for current work or reduce client concurrency; do not
  launch another server against the same socket.
- OpenAI `503 runtime_unavailable`: start the host with `yvex serve`, load the
  selected model with `yvex model load MODEL`, and confirm product and host
  readiness through `yvex model list --json` and `yvex host status --json`.
- OpenAI `422 unsupported_parameter`: remove the named unsupported field;
  fields are never ignored silently.

DeepSeek-specific semantics and source-boundary events are described in the
[family record](../model-families/deepseek-v4-flash.md). Direct component execution,
tokenizer conformance, artifact inspection, and physical-compilation
diagnostics use the advanced `inspect`, `artifact`, `compile`, and `bench`
surfaces in the finite offline lane. Discover them with
`yvex help --advanced`; they are not part of the normal hosted startup path.

The admitted MiniMax-H3 Audio VAE component is reachable through that lane:

```sh
./yvex bench component audio-vae \
  --target minimax-h3-fl2va \
  --artifact /srv/yvex/artifacts/minimax-h3/audio_vae.gguf \
  --backend cuda \
  --input-file /srv/yvex/evidence/audio-latent.f32 \
  --batch 1 \
  --latent-steps 1 \
  --max-device-bytes 2147483648 \
  --out /srv/yvex/evidence/audio-samples.f32
```

The input is contiguous F32 `[batch,32,latent_steps]`; the output is contiguous
mono F32 `[batch,800*latent_steps]` at the source-declared 32 kHz rate. The
command authenticates the exact component artifact, bounds host workspace and
CUDA residency, executes the native decoder, and publishes the output without
replacing an existing file. Use `--backend cpu` and omit
`--max-device-bytes` for the CPU path. Raw component samples are numerical
evidence, not synchronized media or an admitted MiniMax generation path.

The MiniMax-H3 Visual VAE CPU and CUDA component paths are reachable through
the same lane:

```sh
./yvex bench component video-vae \
  --target minimax-h3-fl2va \
  --artifact /srv/yvex/artifacts/minimax-h3/video_vae.gguf \
  --backend cuda \
  --input-file /srv/yvex/evidence/video-latent.f32 \
  --batch 1 \
  --latent-frames 1 \
  --latent-height 1 \
  --latent-width 1 \
  --max-device-bytes 17179869184 \
  --out /srv/yvex/evidence/rgb-frames.f32
```

The input is contiguous F32 `[1,24,T,H,W]`; the output is contiguous F32
`[1,3,T*4,H*16,W*16]`. The three latent dimensions on the command must match
the input file. The command authenticates the complete Visual VAE artifact,
bounds workspace and CUDA residency, executes all 36 native decoder blocks with
exact partial 3D RoPE, and publishes the output without replacing an existing
file. Use `--backend cpu` and omit `--max-device-bytes` for the CPU path. Only
bounded small geometry has live qualification; this does not admit full-scale
or tiled execution. Raw RGB frames are numerical evidence, not a playable video
or synchronized media path.

Decoded video and audio can be synchronized and atomically published through
the native finite offline lane:

```sh
./yvex bench media publish \
  --video-file /srv/yvex/evidence/rgb-frames.f32 \
  --frames 124 --width 32 --height 32 \
  --fps-numerator 24 --fps-denominator 1 \
  --audio-file /srv/yvex/evidence/audio-stereo.f32 \
  --audio-channels 2 --audio-samples 165600 --sample-rate 32000 \
  --max-host-bytes 1073741824 --max-output-bytes 4294967296 \
  --out /srv/yvex/evidence/minimax-h3.avi --output audit
```

The input files are contiguous planar F32 `[3,frames,height,width]` RGB and
`[channels,samples]` PCM. The native AVI writer stores uncompressed BGR24 video
and PCM S16LE audio, trims surplus PCM to the exact rational video duration,
validates the finished RIFF structure, and publishes without replacing an
existing destination. Its identities exclude the local paths. This command is
playable decoded-media publication, not MiniMax prompt-to-video generation; the
model phases remain separate until the end-to-end composition is admitted.
