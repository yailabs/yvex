# YVEX Runtime Filesystem

This document extracts the runtime filesystem contract from `docs/spine.md`.
The spine remains authoritative.

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
command line option
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

Project-local mode is enabled by explicit command option or by setting
`YVEX_RUN_DIR=.yvex/runs`.

## Run Directory

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

## Receipt Rule

`receipt.json` is execution-local evidence. It is not a YAI case record until
YAI imports it.

## Filesystem API

```c
typedef struct yvex_run_dir yvex_run_dir;

typedef struct {
    const char *base_dir;
    const char *command_line;
    int create;
} yvex_run_dir_options;

int yvex_run_dir_open(yvex_run_dir **out, const yvex_run_dir_options *opt, yvex_error *err);
const char *yvex_run_dir_path(const yvex_run_dir *dir);
int yvex_run_dir_write_text(yvex_run_dir *dir, const char *name, const char *data, yvex_error *err);
int yvex_run_dir_append_jsonl(yvex_run_dir *dir, const char *name, const char *line, yvex_error *err);
void yvex_run_dir_close(yvex_run_dir *dir);
```

Path APIs must avoid hidden global state except explicitly controlled config and
logging state.
