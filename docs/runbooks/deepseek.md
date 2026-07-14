# DeepSeek Operator Runbook

Date: 2026-07-14
Status: unsupported current-state boundary

## Current Target

Canonical source: `$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash`.

Canonical v0.1.0 release target: `deepseek4-v4-flash` on DGX Spark CUDA.

The local source has passed exact metadata and header verification against
`deepseek-ai/DeepSeek-V4-Flash` at commit
`60d8d70770c6776ff598c94bb586a859a38244f1`. The pinned upstream index owns 46
shards and 69,187 unique tensor records. Payload trust is separate: a complete
read-only scan checks every authoritative Hugging Face Git LFS SHA-256, seals
the aggregate identity in the verifier-owned manifest, and binds every mapped
contribution to a checked range.

## Current Boundary

YVEX cannot currently produce a complete DeepSeek-V4-Flash GGUF, materialize
the complete model, execute the transformer/MoE stack, or generate text.
There is no supported DeepSeek generation command to run yet.

Selected aliases, bounded graph segments, diagnostic runtime commands,
report-only fullmodel surfaces, fixture logits, and printed token IDs are not a
DeepSeek operator lane. Their dispositions remain in `../../PROJECT.md`.

Current milestone state, dependencies, gates, and Active Next live only in
`../../PROJECT.md`; this file records executable operator procedures.

## Source Verification

The strict metadata/header pass is:

```sh
./yvex source-manifest report \
  --release v0.1.0 \
  --family deepseek \
  --target deepseek4-v4-flash \
  --models-root "$HOME/lab/models" \
  --audit \
  --strict
```

It exits non-zero for provenance, structured-config, index, shard, header, or
footprint contradictions. It may atomically publish only a complete manifest;
it reads no tensor payload and proves no artifact or runtime support.

## Source Payload Verification

Plan every retained tensor range without reading payload bytes:

```sh
make test-source-payload-live-plan
```

Read all 46 shards through the payload API, verify authoritative digests,
deliver every mapped logical range to a counting sink, and atomically publish
the payload identity:

```sh
make test-source-payload-live
```

Both targets default to the canonical root. `DEEPSEEK_SOURCE`,
`DEEPSEEK_MODELS_ROOT`, and `DEEPSEEK_SOURCE_MANIFEST` select an equivalent
explicit snapshot. The full scan reads about 159.6 GB and may take substantial
time. It never writes beneath the source root or prints weight bytes. Reported
timings are diagnostic, not a release benchmark.

This proves source payload trust and bounded transactional delivery only. It
does not transform, quantize, emit GGUF, materialize, or execute model code.

## Canonical Control

- Project control: `../../PROJECT.md`
- Release gates: `../v010-release-doctrine.md`
- Filesystem ownership: `../system-target.md`
- Artifact terminology: `../../MODEL_ARTIFACTS.md`
