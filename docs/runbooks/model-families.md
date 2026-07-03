# Model Family Runbook

## What This File Is

This runbook records model-family pressure lanes that are not yet concrete
model-specific operator paths.

Use `deepseek.md` or `glm.md` when working with those concrete targets. Use this
file when checking family-level pressure lanes such as Qwen on Apple Silicon /
Metal.

## Qwen / Metal Pressure Lane

Purpose:
  Report the planned reduced-scale Qwen portability lane for future Apple
  Silicon / Metal full-runtime work.

Requires:
  Repository root.
  No Qwen weights.
  No Metal backend.
  No generated artifact.

Writes:
  Nothing.

Safe to rerun:
  yes

Stop after:
  the report prints source, hardware, backend, candidate, blocker, and next-row
  fields.

Boundary:
  report-only pressure lane
  no download
  no Qwen artifact emission
  no Qwen tensor materialization
  no Metal backend support
  no graph/runtime execution
  no generation
  no evaluation
  no benchmark
  no release-ready claim

```sh
./yvex model-target qwen-metal --release v0.1.0 --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next
```

The next operator-visible step is a Qwen source pressure report. That report
must ground source/config facts before Qwen/Metal can become a concrete runtime
candidate.
