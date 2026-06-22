# YVEX CLI Runtime

This document owns CLI behavior. YVEX is CLI-only.

## Current Implemented Commands

The H0 binary implements exactly:

```text
yvex
yvex --help
yvex --version
yvex backend cpu|cuda
yvex commands
yvex detokenize <path> --ids IDS
yvex engine <path>
yvex graph <path>
yvex help
yvex help <implemented-command>
yvex info
yvex inspect <path>
yvex metadata <path>
yvex paths
yvex paths --project DIR
yvex paths --run
yvex paths --run --create
yvex plan <path>
yvex prompt <path> --user TEXT
yvex session <path> --backend cpu
yvex tokenizer <path>
yvex tokenize <path> --text TEXT
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
status: H0 engine and session runtime skeleton
library: libyvex.a
filesystem: implemented
artifact: open/read implemented
gguf: metadata/tensor directory parsing implemented
model: descriptor-only implemented
tokenizer: fixture encode/decode implemented
prompt: default renderer implemented
graph: partial planning implemented
planner: estimate-only implemented
backend: CPU reference implemented
engine: runtime object skeleton implemented
session: lifecycle skeleton implemented
kv: unavailable skeleton implemented
logits: unavailable skeleton implemented
backend_cuda: not implemented
inference: not implemented
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

`yvex inspect <path>` opens a file, parses the GGUF header, metadata table and
raw tensor directory, builds a YVEX tensor table and model descriptor, then
prints a descriptor-only summary.

Valid GGUF directory output:

```text
format: gguf
version: 3
metadata_count: 5
tensor_count: 1
tensor_data_offset: 288
alignment: 32
architecture: llama
model_name: yvex-test
known_tensor_bytes: 128
unsupported_tensor_accounting: 0
status: descriptor-only
```

Unknown format output:

```text
format: unknown
status: unsupported
```

`inspect` does not load tokenizer, backend, session, or inference state.

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

`yvex tensors <path>` prints YVEX tensor table rows derived from GGUF tensor
directory records.

Output shape:

```text
format: gguf
version: 3
tensor_count: 1
tensor_data_offset: 288
alignment: 32

0 token_embd.weight role=token_embedding rank=2 dims=[4,8] dtype=F32 bytes=128 offset=0 absolute=288
```

This is descriptor/table data for inspection. It is not backend support, model
loading, or inference state.

## Current `yvex tokenizer`

`yvex tokenizer <path>` prints tokenizer metadata, vocabulary size, special
token IDs, and E0 support posture.

Output shape:

```text
format: gguf
architecture: llama
model_name: yvex-tokenizer-test
tokenizer_model: yvex-fixture-simple
support: fixture-encode-decode
vocab_size: 8
bos_token_id: 1
eos_token_id: 2
unk_token_id: 0
chat_template: absent
status: tokenizer-descriptor
```

Real Llama/GPT2/Replit/RWKV tokenizer algorithms are not executed in E0. Their
metadata and vocab can be inspected when present, but encode/decode returns an
explicit unsupported error unless the algorithm is implemented in a future wave.

## Current `yvex tokenize`

`yvex tokenize <path> --text TEXT` encodes text only when the tokenizer support
level is `fixture-encode-decode`.

Output shape:

```text
tokens: 3
ids: 3 4 5
pieces:
  3 "hello"
  4 " "
  5 "world"
status: tokenized
```

## Current `yvex detokenize`

`yvex detokenize <path> --ids 3,4,5` decodes comma-separated token IDs through
the fixture tokenizer.

Output shape:

```text
text: "hello world"
status: detokenized
```

## Current `yvex prompt`

`yvex prompt <path> --system TEXT --user TEXT` renders explicit role messages
through the YVEX default prompt format. Arbitrary Jinja chat templates are not
executed in E0.

Output shape:

```text
template: yvex-default
chat_template_metadata: absent
rendered_bytes: <n>
rendered:
<system>
You are helpful
</system>
<user>
hello world
</user>
<assistant>
status: rendered
```

With `--tokens`, the rendered prompt is tokenized if the tokenizer support level
is `fixture-encode-decode`.

## Current `yvex graph`

`yvex graph <path>` opens a GGUF artifact, builds the tensor table and model
descriptor, then emits the F0 graph planning artifact. It does not execute ops.

Output shape for the current fixture:

```text
graph status: partial
architecture: llama
model_name: yvex-tokenizer-test
values: 3
ops: 1
missing_required: 2

value 0 token_ids kind=token_ids shape=[1] dtype=I32 residency=host
value 1 token_embd.weight kind=weight shape=[4,8] dtype=F32 residency=host source=token_embd.weight
value 2 hidden kind=activation shape=[1,4] dtype=F32 residency=host

op 0 embed status=planned inputs=[0,1] outputs=[2]

missing output_norm reason="required for final normalization"
missing output_head reason="required for logits"
status: graph-partial
```

Options:

```text
--seq N
--ctx N
```

## Current `yvex plan`

`yvex plan <path>` builds a graph and estimate-only memory plan. In G0 the CPU
backend ABI is available for tensor allocation/read/write/embed, but plan output
still remains non-executable.

Output shape:

```text
plan status: partial
backend: cpu
backend_status: available
backend_capabilities:
  tensor_alloc: yes
  tensor_read_write: yes
  op_embed: yes
  op_matmul: no
  op_rms_norm: no
  op_attention: no
architecture: llama
model_name: yvex-tokenizer-test
graph_status: partial
ops: 1
missing_required: 2

memory:
  model_tensor_bytes_known: 128
  model_tensor_bytes_unknown_count: 0
  activation_peak_bytes: 16
  kv_cache_bytes: 0
  scratch_peak_bytes: 0
  total_known_bytes: 144

execution_ready: false
reason: graph partial; missing output_norm, output_head; backend lacks full graph ops
status: plan-only
```

`--backend cuda` is accepted only to report unsupported status:

```text
backend: cuda
backend_status: unsupported
execution_ready: false
reason: CUDA backend not implemented in G0
status: plan-only
```

## Current `yvex backend`

`yvex backend cpu` opens the CPU reference backend and reports memory stats and
capabilities:

```text
backend: cpu
status: ready
memory:
  allocated_bytes: 0
  allocation_count: 0
  peak_allocated_bytes: 0
capabilities:
  tensor_alloc: yes
  tensor_read_write: yes
  op_embed: yes
  op_matmul: no
  op_rms_norm: no
  op_attention: no
status: backend-ready
```

`yvex backend cuda` reports the unsupported path and exits 5:

```text
backend: cuda
status: unsupported
reason: CUDA backend is planned for L0
status: backend-unsupported
```

## Current `yvex engine`

`yvex engine <path>` opens the H0 engine stack and prints a descriptor/runtime
summary. It does not run prefill, decode, chat, or generation.

```text
engine status: partial
format: gguf
architecture: llama
model_name: yvex-tokenizer-test
metadata_count: <n>
tensor_count: 1
known_tensor_bytes: 128
unsupported_tensor_accounting: 0
tokenizer_model: yvex-fixture-simple
tokenizer_support: fixture-encode-decode
graph_status: partial
execution_ready: false
reason: graph partial; missing output_norm, output_head
status: engine-descriptor
```

## Current `yvex session`

`yvex session <path> --backend cpu` creates a lifecycle-only session over an
engine and CPU backend. It is a runtime object proof, not a generation command.

```text
engine_status: partial
backend: cpu
backend_status: ready
session_state: partial
context_length: 4096
position: 0
accepted_tokens: 0
kv_status: unavailable
kv_bytes: 0
logits_status: unavailable
execution_ready: false
reason: graph partial; missing output_norm, output_head
status: session-created
```

`--text TEXT --accept-tokens` tokenizes fixture text and advances the session
position only through explicit token acceptance:

```text
tokens: 3
accepted_tokens: 3
position: 3
execution_ready: false
status: session-token-accepted
```

`--backend cuda` reports the unsupported future backend path and exits 5:

```text
backend: cuda
backend_status: unsupported
reason: CUDA backend is planned for L0
status: session-backend-unsupported
```

## Future Commands

Future command names are not support claims:

```text
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
