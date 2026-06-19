# YVEX CLI Runtime

This document extracts the CLI runtime contract from `docs/spine.md`. The spine
remains authoritative.

## Doctrine

YVEX is CLI-only.

Allowed terminal surfaces:

```text
plain CLI output
rich CLI output
interactive REPL
streamed token output
inline status lines
structured JSON output
structured JSONL streaming
logs on stderr
run artifacts on filesystem
```

Forbidden:

```text
TUI
terminal dashboard implementation
terminal grid UI
GUI
--tui
--dashboard
```

## Commands

```sh
yvex info
yvex inspect model.gguf
yvex metadata model.gguf
yvex tensors model.gguf
yvex plan model.gguf --backend cuda --ctx 32768
yvex graph model.gguf
yvex tokenize model.gguf --text "hello"
yvex detokenize model.gguf --ids 1,2,3
yvex prompt model.gguf --system "..." --user "..."
yvex experts model.gguf
yvex moe-plan model.gguf --backend cuda
yvex run --model model.gguf --backend cuda -p "Explain mmap in C" -n 128
yvex chat --model model.gguf --backend cuda
yvex bench --model model.gguf --backend cuda --prompt prompts/code.txt --tokens 256
yvex cuda-info
```

Commands are support claims only after source implementation, tests, manual
proof, and documented limitations exist.

## stdout/stderr

```text
stdout:
  generated text or machine-readable response

stderr:
  status
  progress
  logs
  warnings
  timing
```

## Exit Codes

```text
0   success
1   generic error
2   invalid command line arguments
3   filesystem/artifact error
4   format/parser error
5   unsupported model/qtype/backend feature
6   backend initialization or execution error
7   cancelled/interrupted
8   validation/test failure
9   internal invariant/state error
```

## Argument Grammar

```text
yvex <command> [positional...] [options...]
```

Unknown options, missing option values, and ambiguous positional arguments exit
with code 2.

## TTY and Pipe Safety

```text
color auto-enables only when output target is a TTY
status line auto-enables only on TTY stderr
JSON and JSONL modes never emit color
generated text or machine-readable data stays on stdout
progress/logs stay on stderr
```

Default/plain mode must allow:

```sh
yvex run ... > output.txt
```

without status/progress corrupting `output.txt`.

## JSON and JSONL Versions

JSON envelope:

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "inspect",
  "status": "ok",
  "data": {},
  "error": null
}
```

JSONL event envelope:

```json
{
  "schema": "yvex.event.v1",
  "event": "token",
  "run_id": "run_...",
  "ts_ns": 0,
  "data": {}
}
```

## Interactive Chat

Initial REPL dependency:

```text
linenoise
```

Slash commands:

```text
/help
/model
/backend
/ctx N
/tokens N
/temp F
/top-k N
/top-p F
/min-p F
/seed N
/greedy
/stats
/memory
/tensors
/graph
/trace on
/trace off
/profile
/dump-tokens
/dump-logprobs N
/save-run
/reset
/rewind N
/read FILE
/quit
```

Cancellation and rollback behavior are defined in `docs/spine.md`.
