# YVEX CLI Runtime

This document owns CLI behavior. YVEX is CLI-only.

## Current Implemented Commands

The C1 binary implements exactly:

```text
yvex
yvex --help
yvex --version
yvex commands
yvex help
yvex help <implemented-command>
yvex info
yvex inspect <path>
yvex metadata <path>
yvex paths
yvex paths --project DIR
yvex paths --run
yvex paths --run --create
yvex tensors <path>
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
status: C1 GGUF metadata/tensor directory parser
library: libyvex.a
filesystem: implemented
artifact: open/read implemented
inference: not implemented
gguf: metadata/tensor directory parsing implemented
cuda: not implemented
server: not implemented
```

It reports only implemented surfaces.

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

## Current `yvex inspect`

`yvex inspect <path>` opens a file, parses the GGUF header, metadata table, and
raw tensor directory, then prints a directory-only summary.

Valid GGUF directory output:

```text
format: gguf
version: 3
metadata_count: 5
tensor_count: 1
tensor_data_offset: 288
alignment: 32
status: directory-only
```

Unknown format output:

```text
format: unknown
status: unsupported
```

`inspect` does not load a model descriptor, tokenizer, backend, session, or
inference state.

## Current `yvex metadata`

`yvex metadata <path>` prints parsed GGUF metadata entries.

Output shape:

```text
format: gguf
version: 3
metadata_count: 5

general.architecture = "llama"
general.name = "yvex-test"
llama.context_length = 4096
general.file_type = 0
general.alignment = 32
```

Strings are quoted. Arrays are summarized as `array<type>[count]` rather than
dumped fully.

## Current `yvex tensors`

`yvex tensors <path>` prints raw GGUF tensor directory records.

Output shape:

```text
format: gguf
version: 3
tensor_count: 1
tensor_data_offset: 288
alignment: 32

0 token_embd.weight rank=2 dims=[4,8] type=F32 offset=0 absolute=288
```

This is raw GGUF directory data. It is not a tensor table, dtype/qtype support
claim, backend support claim, or model loading claim.

## Future Commands

Future command names are not support claims:

```text
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

C1 currently exercises `0`, `2`, `4`, and `5`; artifact IO failures map to `3`.

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

## Interface Policy

```text
YVEX is CLI-only.
The only user-facing executable surface in the current repository is build/bin/yvex.
New interface surfaces require an explicit roadmap decision before implementation.
```
