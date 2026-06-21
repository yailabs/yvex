# YVEX CLI Runtime

This document owns CLI behavior. YVEX is CLI-only.

## Current Implemented Commands

The B0 binary implements exactly:

```text
yvex
yvex --help
yvex --version
yvex commands
yvex help
yvex help <implemented-command>
yvex info
yvex paths
yvex paths --project DIR
yvex paths --run
yvex paths --run --create
yvex version
```

Current commands must stay backed by the command table in `cli/yvex_cli.c`.

## Current `yvex info`

`yvex info` must report the current implementation honestly:

```text
name: YVEX
version: 0.1.0
language: C
interface: CLI-only
status: B0 runtime filesystem skeleton
library: libyvex.a
filesystem: implemented
inference: not implemented
gguf: not implemented
cuda: not implemented
server: not implemented
```

It must not include a TUI line.

## Command Table Policy

```text
implemented commands are declared in one command table
unknown commands exit 2
help for unknown topics exits 2
future runtime commands are not listed as implemented
future runtime commands may be documented as future only
```

## Current `yvex paths`

`yvex paths` prints resolved runtime directories:

```text
config: /home/user/.config/yvex
cache: /home/user/.cache/yvex
state: /home/user/.local/state/yvex
data: /home/user/.local/share/yvex
project:
```

`yvex paths --project DIR` switches to explicit project-local mode:

```text
config: DIR/.yvex
cache: DIR/.yvex/cache
state: DIR/.yvex/state
data: DIR/.yvex/data
project: DIR/.yvex
```

`yvex paths --run` prepares run-directory paths without creating them.
`yvex paths --run --create` creates the run root and run directory.

## Future Commands

Future command names are not support claims:

```text
yvex inspect model.gguf
yvex metadata model.gguf
yvex tensors model.gguf
yvex plan model.gguf --backend cuda --ctx 32768
yvex graph model.gguf
yvex tokenize model.gguf --text "hello"
yvex detokenize model.gguf --ids 1,2,3
yvex prompt model.gguf --system "..." --user "..."
yvex run --model model.gguf --backend cuda -p "Explain mmap in C" -n 128
yvex chat --model model.gguf --backend cuda
yvex bench --model model.gguf --backend cuda --prompt prompts/code.txt --tokens 256
yvex cuda-info
```

Each future command needs source implementation, tests, manual proof, failure
behavior, and documented limitations before appearing in `yvex commands`.

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

Default/plain mode must allow:

```sh
yvex run ... > output.txt
```

without status or progress corrupting `output.txt`.

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

B0 currently exercises `0`, `2`, and filesystem/path errors mapped to `3`.

## TTY And Pipe Safety

```text
color auto-enables only when output target is a TTY
status line auto-enables only on TTY stderr
JSON and JSONL modes never emit color
generated text or machine-readable data stays on stdout
progress/logs stay on stderr
status line uses carriage return only on TTY or explicit always mode
final status line is followed by newline before exit
```

## Layout Examples

Future rich run layout:

```text
YVEX 0.1.0 | backend: cuda | device: NVIDIA GB10 | model: qwen

LOAD
  file        models/qwen.gguf
  format      GGUF v3
  tensors     812

MEMORY PLAN
  weights     mmap
  kv cache    planned
  scratch     planned

ASSISTANT
  ...
```

Future status line:

```text
[decode] 87 tok | 18.7 tok/s | last 52ms | ctx 1192/32768
```

These are future layouts, not current command output.

## JSON And JSONL Future Policy

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
{"schema":"yvex.event.v1","event":"token","run_id":"run_...","ts_ns":0,"data":{}}
```

JSONL output must remain one valid JSON object per line. JSON and JSONL modes
must never mix logs or progress into stdout.

## No TUI

Forbidden:

```text
TUI
terminal dashboard implementation
terminal grid UI
GUI
--tui
--dashboard
panel implementation
alternate-screen interface
ncurses dependency
```
