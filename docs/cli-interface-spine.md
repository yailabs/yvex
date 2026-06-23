# YVEX CLI Interface Spine

Date: 2026-06-23
Status: canonical focused interface spine
Scope: CLI layout, REPL behavior, one-shot diagnostics, server/operator UX

## 1. Purpose

This document defines how YVEX should feel and behave as a command-line tool.

It is not the command reference.
It is not the runtime implementation plan.
It is not the server API reference.

It owns the interface shape:

```text
./yvex
./yvex <command>
./yvexd
```

## 2. Interface Principle

YVEX must have at most two human-facing CLI modes:

```text
1. Interactive REPL
2. One-shot diagnostic/tool commands
```

`yvexd` is not a third human mode. It is the provider/server daemon.

## 3. Canonical Entrypoints

Repository-local:

```sh
./yvex
./yvexd
```

Global, if installed or linked:

```sh
yvex
yvexd
```

Compiled binaries:

```text
build/bin/yvex
build/bin/yvexd
```

Human entrypoints should be:

```text
./yvex   -> interactive or one-shot CLI
./yvexd  -> daemon/provider shell
```

## 4. Target CLI Shape

### Interactive

Eventually:

```sh
./yvex
```

opens:

```text
YVEX - YAI Vector Execution
mode: console
model: none
backend: none
server: disconnected
execution_ready: false
generation: unsupported

yvex>
```

### One-shot diagnostics

```sh
./yvex inspect FILE
./yvex tensors FILE
./yvex materialize --model FILE --backend cuda
./yvex model-gate check ...
./yvex materialize-gate check ...
```

### Server/provider

```sh
./yvexd --host 127.0.0.1 --port 18080 --model FILE --backend cuda
```

## 5. Command Role Taxonomy

### Primary future interactive command

```text
yvex console / yvex chat / ./yvex with no args
```

Final decision target:

```text
./yvex with no args opens the canonical REPL.
```

### One-shot diagnostics

```text
inspect
metadata
tensors
tokenizer
tokenize
detokenize
prompt
graph
plan
backend
cuda-info
materialize
model-gate
materialize-gate
convert
source-manifest
native-weights
tensor-map
quant-policy
imatrix
qtype-support
quant-job
```

### Server/daemon

```text
yvexd
```

### Accepted-only legacy/diagnostic path

```text
run
```

`run` is accepted-only until real prefill/decode exists. It must not be
presented as a full execution command.

## 6. `prompt` Command Policy

`prompt` is a renderer/diagnostic command.

It answers:

```text
How does this tokenizer/template render this input?
```

It does not mean:

```text
send this prompt to a model and generate
```

`prompt` remains one-shot diagnostic tooling.

## 7. `run` Command Policy

`run` is currently accepted-only diagnostic tooling.

It may later become a true one-shot request command only after:

```text
prefill implemented
decode implemented
logits valid
sampler implemented
```

Until then, docs must describe it as:

```text
accepted-only diagnostic runtime path
```

not:

```text
model execution
```

## 8. `chat` / REPL Policy

The current `chat` command should evolve into the canonical interactive
console.

The console must support:

```text
line editing
history
slash commands
status banner
model/backend state
server state
logs pointer
clear exit behavior
diagnostic output separation
```

It must not fake generation before inference exists.

## 9. REPL Layout

Startup banner:

```text
YVEX - YAI Vector Execution
version: <version>
mode: console
model: <none|path>
backend: <none|cpu|cuda>
server: <disconnected|connected>
execution_ready: false
generation: unsupported

yvex>
```

Prompt forms:

```text
yvex>
yvex:cpu>
yvex:cuda>
deepseek4:selected>
yvexd:127.0.0.1:18080>
```

Rules:

```text
Prompt must show enough state to avoid accidental confusion.
Prompt must not imply inference if execution_ready is false.
Prompt must remain short.
```

## 10. Slash Command Vocabulary

Minimum future slash commands:

```text
/help
/status
/model
/load <gguf>
/backend <cpu|cuda>
/inspect
/tensors
/materialize
/model-gate
/materialize-gate
/server
/connect <url>
/logs
/clear
/exit
```

Optional later:

```text
/save
/history
/reset
/profile
/trace
```

Slash commands must be registry-driven, not ad hoc string handling.

## 11. Free Text Input Policy

When user enters plain text before inference exists:

```text
accepted_input: true
decode: unsupported
generation: unsupported
execution_ready: false
reason: decode runtime is not implemented
```

Do not emit fake assistant text.

When inference exists later, the same free-text path becomes generation input.

## 12. Line Editing Boundary

YVEX may support a line editor similar to common minimal REPL tools.

Boundary:

```text
line editing belongs only to the CLI/console layer
line editing must not enter libyvex public API
line editing must not enter session/backend/runtime core
server/headless use must not require line editing
```

Implementation strategy:

```text
default: stdio fallback
optional: external line editor adapter
```

Possible future adapters:

```text
stdio
linenoise-like
readline/libedit-like
```

The internal boundary should look like:

```c
typedef struct yvex_line_editor yvex_line_editor;

int yvex_line_editor_open(...);
char *yvex_line_editor_readline(...);
void yvex_line_editor_add_history(...);
void yvex_line_editor_close(...);
```

This interface should remain internal to CLI/console code.

## 13. History Policy

Future REPL history:

```text
history file optional
history stored under runtime/user state path
no secrets
no prompt bodies if privacy mode disabled/unclear
clear command available
```

Default conservative posture:

```text
history off or session-local until explicit history policy exists
```

## 14. Logs Policy

Server logs are owned by `yvexd`.

CLI console may display or tail log locations, but should not mix server logs
into the REPL output stream unless explicitly requested.

Recommended UX:

```text
terminal 1: yvexd server
terminal 2: tail logs
terminal 3: yvex console/client
```

## 15. Output Streams

Rules:

```text
stdout:
  primary command output
  machine-readable output when requested

stderr:
  errors
  diagnostics
  debug/status lines if not part of payload

logs:
  server lifecycle/request logs
  optional run artifacts
```

## 16. No False Claims

The console must never claim:

```text
full model support
inference ready
generation ready
OpenAI compatibility
benchmark performance
execution_ready: true
```

until those are implemented and proven.

## 17. Future Implementation Ladder

```text
CLI.CONSOLE.0 - Canonical REPL layout and command taxonomy
CLI.CONSOLE.1 - Line editor internal adapter and stdio fallback
CLI.CONSOLE.2 - Slash command registry cleanup
CLI.CONSOLE.3 - Stateful console session over local artifact
CLI.CONSOLE.4 - Server-connected console mode
CLI.CONSOLE.5 - True generation console after prefill/decode/sampler
```

Only `CLI.CONSOLE.0` is documentation/design. Later waves implement behavior.

## 18. Relationship To M Ladder

M2/M3/M4 focus on materialization and runtime attachment.
CLI.CONSOLE waves must not imply runtime support that M waves have not
implemented.

REPL may expose runtime state, but must not create fake runtime state.

## 19. Acceptance For CLI.CONSOLE.0

```text
docs/cli-interface-spine.md exists
REPL/one-shot/server roles are clear
prompt/run/chat ambiguity is resolved at design level
line editor boundary is explicit
no code behavior changes
no new dependency
no inference claim
```
