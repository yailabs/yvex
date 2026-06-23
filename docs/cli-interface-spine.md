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

## Model Selection Doctrine

YVEX must support three ways to identify a model artifact:

```text
1. absolute or relative path
2. local model alias
3. current selected model in the repository/user runtime state
```

Path examples:

```sh
./yvex inspect /absolute/path/model.gguf
./yvex inspect ./relative/model.gguf
```

Alias examples:

```sh
./yvex inspect --model deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvexd --model deepseek4-v4-flash-selected-embed --backend cuda
```

Current selected model examples:

```sh
./yvex models use deepseek4-v4-flash-selected-embed
./yvex
```

Inside REPL:

```text
yvex> /model
deepseek4-v4-flash-selected-embed

yvex> /use deepseek4-v4-flash-selected-embed
model selected
```

Model selection is persistent local operator state. It is not committed to git.

## Local Model Registry

YVEX model artifacts live outside the repository.

Recommended external roots:

```text
~/lab/models/gguf/
~/lab/models/hf/
```

YVEX may keep a local registry under repository/user runtime state.

Preferred local registry path:

```text
.yvex/models.local.json
```

or, if user-level state is preferred:

```text
~/.local/share/yvex/models.json
```

Policy:

```text
models.local.json is machine-local
models.local.json is not committed
absolute paths are allowed in local registry
tracked docs may show examples but must not require the user's exact machine path
```

The registry maps aliases to artifacts:

```json
{
  "schema": "yvex.models.local.v1",
  "selected": "deepseek4-v4-flash-selected-embed",
  "models": [
    {
      "alias": "deepseek4-v4-flash-selected-embed",
      "family": "deepseek4",
      "model": "v4-flash",
      "scope": "selected",
      "artifact_class": "embed",
      "qprofile": "F16",
      "calibration": "noimatrix",
      "producer": "yvex",
      "schema_version": "v1",
      "path": "/home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf",
      "sha256": "5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab",
      "support_level": "selected-tensor-materialized",
      "execution_ready": false
    }
  ]
}
```

Current implementation:

```text
CLI.MODELS.0 documented the registry.
CLI.MODELS.1 implements the local registry and `yvex models` command group.
CLI.MODELS.1 default path is `.yvex/models.local.json`.
CLI.MODELS.1 supports `--registry FILE` and `YVEX_MODELS_REGISTRY`.
CLI.MODELS.2 will wire aliases into one-shot commands.
```

## Model Alias Rules

Aliases must be stable, lowercase, and human-readable.

Alias grammar:

```text
<family>-<model>-<scope>-<artifact-class>
```

Examples:

```text
deepseek4-v4-flash-selected-embed
qwen3-8b-selected-embed
```

Aliases must not include:

```text
absolute paths
spaces
latest
final
new
test
```

The artifact filename remains more precise:

```text
deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

The alias is shorter:

```text
deepseek4-v4-flash-selected-embed
```

The registry holds the exact path, hash, support level, and artifact metadata.

## Model Discovery Rules

Implemented command:

```sh
./yvex models scan
```

Expected scan roots:

```text
1. --root DIR if provided
2. YVEX_MODELS_DIR if set
3. .yvex/models/ if present
4. ~/lab/models/gguf/ if present
5. ~/.local/share/yvex/models/ if present
```

Scanning must look for:

```text
*.gguf
```

For each GGUF:

```text
parse filename if it follows YVEX artifact naming grammar
run lightweight inspect
extract tensor_count, architecture/model if available
compute sha256 only if explicitly requested or during registration
```

Scanning must not:

```text
materialize automatically
execute model
generate text
modify files
```

## `yvex models` Command

Implemented command group:

```text
yvex models scan
yvex models list
yvex models add
yvex models remove
yvex models use
yvex models current
yvex models inspect
```

### `models scan`

```sh
./yvex models scan --root ~/lab/models/gguf
```

Finds candidate GGUF files.

### `models add`

```sh
./yvex models add \
  --alias deepseek4-v4-flash-selected-embed \
  --path ~/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Adds a model to local registry.

### `models list`

```sh
./yvex models list
```

Expected:

```text
* deepseek4-v4-flash-selected-embed
  family: deepseek4
  scope: selected
  qprofile: F16
  support: selected-tensor-materialized
  execution_ready: false
  path: /home/.../deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

### `models use`

```sh
./yvex models use deepseek4-v4-flash-selected-embed
```

Sets the current selected model for the local user/repo.

### `models current`

```sh
./yvex models current
```

Prints selected model state.

### `models inspect`

```sh
./yvex models inspect deepseek4-v4-flash-selected-embed
```

Runs inspect through alias resolution.

## Model Resolution Rules

Any command that accepts `--model` should eventually resolve:

```text
--model /path/to/file.gguf
--model relative/path.gguf
--model alias
```

Resolution order:

```text
1. existing path
2. registered alias
3. error with suggestions
```

If no model is passed:

```text
one-shot command:
  fail with clear message unless command supports current model

interactive REPL:
  use current selected model if available
  otherwise start with model: none and prompt user to select
```

Error example:

```text
model not found: deepseek
available models:
  deepseek4-v4-flash-selected-embed
hint:
  ./yvex models list
```

## REPL Model Selection

The REPL should not ask for a model on every prompt.

Startup:

```text
YVEX - YAI Vector Execution
model: deepseek4-v4-flash-selected-embed
backend: cuda
execution_ready: false

yvex:cuda>
```

If no model selected:

```text
YVEX - YAI Vector Execution
model: none
backend: none
execution_ready: false

No model selected.
Use /models to list available models or /use <alias>.

yvex>
```

Slash commands:

```text
/models
/model
/use <alias>
/inspect
/tensors
/materialize
/model-gate
/materialize-gate
```

Rules:

```text
model is session state inside REPL
model can be changed explicitly with /use
plain text input never changes model
server connection never silently changes local selected model
```

## Server Model Selection

`yvexd` should accept either path or alias:

```sh
./yvexd --model deepseek4-v4-flash-selected-embed --backend cuda
./yvexd --model /absolute/path/model.gguf --backend cuda
```

If alias is used, yvexd resolves it through the same local model registry.

If model is missing:

```text
server start fails
no background daemon remains running
error includes available model aliases when possible
```

`yvexd` must not automatically pick an arbitrary model unless explicitly
configured.

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
/models
/model
/use <alias>
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
CLI.MODELS.0 - Local model registry and selection spine
CLI.MODELS.1 - Local model registry implementation
CLI.MODELS.2 - Model alias resolution for one-shot commands
CLI.MODELS.3 - Model selection in canonical REPL
CLI.MODELS.4 - Model alias resolution in yvexd
CLI.CONSOLE.0 - Canonical REPL layout and command taxonomy
CLI.CONSOLE.1 - Line editor internal adapter and stdio fallback
CLI.CONSOLE.2 - Slash command registry cleanup
CLI.CONSOLE.3 - Stateful console session over local artifact
CLI.CONSOLE.4 - Server-connected console mode
CLI.CONSOLE.5 - True generation console after prefill/decode/sampler
```

`CLI.MODELS.0` and `CLI.CONSOLE.0` are documentation/design. CLI.MODELS.1
implements registry behavior. Later CLI.MODELS and CLI.CONSOLE waves extend
alias resolution and interactive behavior.

## 18. Relationship To M Ladder

M2/M3/M4 focus on materialization and runtime attachment.
CLI.MODELS and CLI.CONSOLE waves must not imply runtime support that M waves
have not implemented.

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

## 20. Acceptance For CLI.MODELS.0

```text
local model registry doctrine exists
model alias grammar exists
model discovery rules exist
model resolution rules exist
REPL model selection behavior is designed
yvexd alias behavior is designed
no code behavior changes
no registry files committed
no inference claim
```

## 21. Acceptance For CLI.MODELS.1

```text
yvex models command group exists
models scan lists canonical GGUF candidates
models add writes local registry entries
models list prints registry entries
models use selects an alias
models current prints selected alias or none
models inspect resolves alias and runs lightweight GGUF inspect
models remove deletes an alias and clears selected state when needed
.yvex/models.local.json remains local and untracked
one-shot command alias resolution remains future CLI.MODELS.2 work
no inference claim
```
