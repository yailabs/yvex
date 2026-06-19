# YVEX Runtime Filesystem

This document owns the future B0 filesystem contract. No runtime filesystem
implementation exists before B0.

## Status

```text
path resolution: not implemented
project-local .yvex: not implemented
run directory creation: not implemented
lock/cache policy: not implemented
runtime filesystem public API: not implemented
```

## Environment Variables

```text
YVEX_HOME
YVEX_CONFIG_DIR
YVEX_CACHE_DIR
YVEX_STATE_DIR
YVEX_RUN_DIR
```

## Configuration Precedence

Configuration resolves in this order:

```text
CLI option
environment variable
project-local .yvex/config.toml
user config ~/.config/yvex/config.toml
built-in default
```

Commands must report the resolved value source when a path affects run
artifacts, model loading, cache, or state.

## Default Directories

```text
~/.config/yvex/
  config.toml
  models.toml
  backends.toml

~/.cache/yvex/
  tokenizer/
  tensor-plans/
  graph-plans/
  compiled-kernels/
  downloads/

~/.local/state/yvex/
  runs/
  servers/
  locks/
  chat_history

~/.local/share/yvex/
  models/
  fixtures/
```

## Project-Local Mode

```text
.yvex/
  config.toml
  cache/
  runs/
  chat_history
```

Project-local mode is enabled only by explicit command option or by setting
`YVEX_RUN_DIR=.yvex/runs`.

## Run Directory Plan

Serious runs may emit:

```text
runs/YYYY-MM-DD/run_YYYYMMDD_HHMMSS_shortid/
  command.txt
  env.txt
  run.json
  model.json
  backend.json
  memory-plan.json
  graph-plan.txt
  graph-plan.json
  prompt.rendered.txt
  prompt.tokens.jsonl
  output.txt
  decode.tokens.jsonl
  logits/
  logprobs.jsonl
  metrics.json
  profile.json
  trace.jsonl
  receipt.json
  stderr.log
  stdout.log
```

## Lock And Cache Policy

```text
cache files are derived, not authoritative
partial cache writes use temp-and-rename
locks are scoped to cache/run directory mutation
stale locks must be reported with path and owner data when available
cache invalidation must name the version/key that changed
```

## Receipt Rule

`receipt.json` is execution-local evidence. It is not a YAI case record until
YAI imports it through its own authority path.

## B0 Acceptance

B0 must add tests for path precedence, project-local mode, run-directory
creation, and failure behavior. It must not implement inference, GGUF parsing,
tokenization, CUDA, server/provider behavior, or TUI behavior.
