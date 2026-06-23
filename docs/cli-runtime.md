# YVEX CLI Runtime

This document owns CLI behavior. YVEX is CLI-only.

## Current Implemented Commands

The M0 command-line surfaces implement exactly:

```text
yvex
yvex --help
yvex --version
yvex backend cpu|cuda
yvex commands
yvex cuda-info
yvex chat --model FILE --backend cpu|cuda
yvex detokenize <path> --ids IDS
yvex engine <path>
yvex graph <path>
yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--overwrite]
yvex gguf-template inspect --template FILE
yvex gguf-template validate --template FILE
yvex gguf-template compare --template FILE --native-source DIR
yvex help
yvex help <implemented-command>
yvex imatrix create --name NAME --arch NAME --imatrix FILE --format FORMAT --status STATUS --out FILE
yvex imatrix inspect --manifest FILE
yvex imatrix validate --manifest FILE
yvex info
yvex inspect <path>
yvex materialize --model FILE --backend cpu|cuda
yvex metadata <path>
yvex native-weights --source DIR [--limit N] [--tensor NAME] [--json]
yvex paths
yvex paths --project DIR
yvex paths --run
yvex paths --run --create
yvex plan <path>
yvex prompt <path> --user TEXT
yvex quant-policy inspect --policy FILE
yvex quant-policy validate --policy FILE [--template FILE]
yvex quant-policy derive --template FILE --arch NAME --out FILE
yvex run --model FILE --backend cpu|cuda --prompt TEXT
yvex session <path> --backend cpu|cuda
yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE
yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]
yvex tokenizer <path>
yvex tokenize <path> --text TEXT
yvex tensors <path>
yvex version
yvexd --help
yvexd --version
yvexd --host HOST --port PORT [--model FILE] [--backend cpu|cuda] [--one-request]
```

Current commands must stay backed by the command table in `cli/yvex_cli.c`.

## Current `yvex info`

`yvex info` must report the current implementation honestly:

```text
name: YVEX
version: 0.1.0
language: C
interface: CLI-only
status: M0 fixture weight materialization
library: libyvex.a
filesystem: implemented
artifact: open/read implemented
gguf: metadata/tensor directory parsing implemented
gguf_emit: controlled GGUF writer implemented
model: descriptor-only implemented
tokenizer: fixture encode/decode implemented
prompt: default renderer implemented
graph: partial planning implemented
planner: estimate-only implemented
backend: CPU reference implemented
backend_cuda: tensor movement and F32 embed implemented when CUDA is available
weights: fixture materialization implemented
engine: runtime object skeleton implemented
session: lifecycle skeleton implemented
run: accepted-only runtime shell implemented
chat: accepted-only REPL shell implemented
metrics: runtime collector implemented
trace: JSONL writer implemented
profile: JSON writer implemented
run_artifacts: metrics/trace/profile files implemented
source_manifest: provenance JSON writer implemented
native_weights: safetensors header inventory implemented
gguf_template: contract validator implemented
weight_mapping: tensor adapter contract implemented
quant_policy: manifest validator implemented
server_binary: yvexd shell implemented
server_endpoints: health/metrics/models status implemented
server_generation: not implemented
kv: unavailable skeleton implemented
logits: unavailable skeleton implemented
generation: unsupported
inference: not implemented
cuda: available when local driver/device probe succeeds
server: yvexd status shell implemented
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

## Current `yvex source-manifest`

`yvex source-manifest create` writes an OWI.1 source provenance JSON manifest
for a local official-weight source tree. It scans file paths, byte sizes, and
coarse file kinds only. It does not download, parse safetensors, quantize, emit
GGUF, materialize, or infer.

Required options:

```text
--hf-repo REPO
--revision REV
--local-path DIR
--status unknown|in-progress|incomplete|complete|failed
--out FILE
```

Optional provenance options:

```text
--license TEXT
--model-card URL
--node NAME
--dry-run-log FILE
--download-log FILE
--pid-file FILE
--download-command TEXT
```

DeepSeek in-progress manifest command shape:

```sh
mkdir -p "$HOME/lab/manifests/deepseek"

./build/bin/yvex source-manifest create \
  --hf-repo deepseek-ai/DeepSeek-V4-Flash \
  --revision main \
  --license MIT \
  --model-card https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash \
  --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --node spark \
  --status in-progress \
  --dry-run-log "$HOME/lab/artifacts/download-logs/deepseek-v4-flash-dry-run.txt" \
  --download-log "$HOME/lab/artifacts/download-logs/deepseek-v4-flash-download.log" \
  --pid-file "$HOME/lab/artifacts/download-logs/deepseek-v4-flash-download.pid" \
  --download-command "hf download deepseek-ai/DeepSeek-V4-Flash ..." \
  --out "$HOME/lab/manifests/deepseek/deepseek-v4-flash-source-manifest.json"
```

Real source manifests and model files live outside the repository unless a
future spine decision explicitly changes that policy.

## Current `yvex native-weights`

`yvex native-weights --source DIR` inventories safetensors headers under a local
source tree. It reads metadata only: tensor names, shard paths, dtypes, shapes,
payload offsets, and byte sizes. It does not read tensor payloads, convert,
quantize, emit GGUF, materialize, or infer.

Options:

```text
--source DIR
--limit N
--tensor NAME
--json
```

Output with no completed safetensors yet:

```text
native weights: safetensors
source: /home/dgmothx/lab/models/hf/deepseek/DeepSeek-V4-Flash
shards: 0
tensors: 0
total_tensor_bytes: 0
unknown_dtype_count: 0
malformed_shard_count: 0

status: native-weights-empty
```

`native-weights-empty` is valid while a Hugging Face download is still in
progress and no final `.safetensors` shard has been renamed into place. Once
shards exist, the same command reports tensor rows and exits with
`status: native-weights`.

DeepSeek in-progress proof command:

```sh
./build/bin/yvex native-weights \
  --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --limit 40
```

## Current `yvex gguf-template`

`yvex gguf-template` validates a GGUF template as a structural contract for
future conversion/emission. It does not emit GGUF, quantize, materialize, or
infer.

Commands:

```text
yvex gguf-template inspect --template FILE
yvex gguf-template validate --template FILE
yvex gguf-template compare --template FILE --native-source DIR
```

`compare` uses OWI.2 native inventory and performs exact-name comparison only.
Architecture-specific tensor-name mapping belongs to OWI.4, so mismatches are
reported as partial rather than treated as DeepSeek support failure unless
`--require-all-template-tensors-in-native` is passed.

## Current `yvex gguf-emit`

`yvex gguf-emit controlled` writes the first YVEX-owned GGUF artifact from a
controlled source: native `embed.weight`, target `token_embd.weight`, F32,
dimensions `[4,8]`, 128 payload bytes, alignment 32, and controlled tokenizer
metadata. It validates the emitted file through the existing parser and CPU
materialization path. It does not convert DeepSeek, quantize, generate imatrix
data, infer, or emit a generic model.

Command:

```sh
./build/bin/yvex gguf-emit controlled \
  --out build/tests/gguf-emit/yvex-owned.gguf \
  --model-name yvex-owned-gguf-test \
  --arch llama \
  --overwrite
```

Roundtrip proof:

```sh
./build/bin/yvex inspect build/tests/gguf-emit/yvex-owned.gguf
./build/bin/yvex metadata build/tests/gguf-emit/yvex-owned.gguf
./build/bin/yvex tensors build/tests/gguf-emit/yvex-owned.gguf
./build/bin/yvex materialize --model build/tests/gguf-emit/yvex-owned.gguf --backend cpu
```

Expected summary:

```text
gguf emit: controlled
metadata_count: 12
tensor_count: 1
tensor_payload_bytes: 128
alignment: 32
roundtrip_validated: yes
status: gguf-written
```

## Current `yvex tensor-map`

`yvex tensor-map` maps native safetensors tensor names to canonical YVEX roles
and proposed GGUF/template tensor names through an architecture adapter. It reads
metadata only. It does not load payload bytes, convert tensors, quantize, emit
GGUF, materialize weights, or infer.

Command shape:

```text
yvex tensor-map --arch deepseek4 --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]
```

DeepSeek provider-node proof shape:

```sh
./build/bin/yvex tensor-map \
  --arch deepseek4 \
  --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --template "/home/dgmothx/lab/src/ds4/gguf/DeepSeek-V4-Flash-IQ2XXS-w2Q2K-AProjQ8-SExpQ8-OutQ8-chat-v2-imatrix.gguf" \
  --tensor embed.weight
```

Expected contract-level result:

```text
native=embed.weight
role=token_embedding
target=token_embd.weight
status=mapped
native_shape=[129280,4096]
target_shape=[4096,129280]
transform=transpose
```

The transpose is a compatibility report only. OWI.4 does not transpose payload
bytes or produce a GGUF.

## Current `yvex quant-policy`

`yvex quant-policy` handles declarative qtype policy manifests. It can inspect a
policy, validate it, or derive a policy from a GGUF template tensor table. It
does not quantize payloads, emit GGUF, run imatrix calibration, materialize
weights, or infer.

Commands:

```text
yvex quant-policy inspect --policy FILE
yvex quant-policy validate --policy FILE [--template FILE]
yvex quant-policy derive --template FILE --arch NAME --out FILE
```

DeepSeek DS4-template proof shape:

```sh
./build/bin/yvex quant-policy derive \
  --template "$DS_TEMPLATE" \
  --arch deepseek4 \
  --out "$HOME/lab/manifests/deepseek/deepseek-v4-flash-quant-policy.json"

./build/bin/yvex quant-policy validate \
  --policy "$HOME/lab/manifests/deepseek/deepseek-v4-flash-quant-policy.json" \
  --template "$DS_TEMPLATE"
```

Validation may be partial when a qtype is storage-known but compute-unsupported
or when storage accounting is not implemented for a qtype.

## Current `yvex imatrix`

`yvex imatrix` creates, inspects, and validates imatrix provenance manifests. A
manifest links an external calibration artifact to optional source-manifest and
quant-policy paths. It checks file presence and counts policy rules that declare
`requires_imatrix=true`. It does not generate imatrix data, run calibration,
quantize payloads, emit GGUF, materialize weights, or infer.

Commands:

```text
yvex imatrix create --name NAME --arch NAME --imatrix FILE --format FORMAT --status STATUS --out FILE
yvex imatrix inspect --manifest FILE
yvex imatrix validate --manifest FILE
```

DeepSeek DS4 external-artifact proof shape:

```sh
./build/bin/yvex imatrix create \
  --name deepseek-v4-flash-ds4-routed-moe-imatrix \
  --arch deepseek4 \
  --source-manifest "$HOME/lab/manifests/deepseek/deepseek-v4-flash-source-manifest.json" \
  --quant-policy "$HOME/lab/manifests/deepseek/deepseek-v4-flash-quant-policy.json" \
  --imatrix "/home/dgmothx/lab/src/ds4/gguf/DeepSeek-V4-Flash-chat-v2-routed-moe-ds4.dat" \
  --format ds4_routed_moe_dat \
  --status present \
  --dataset "ds4 calibration dataset" \
  --producer ds4 \
  --out "$HOME/lab/manifests/deepseek/deepseek-v4-flash-imatrix-manifest.json"
```

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

## Current `yvex materialize`

`yvex materialize --model FILE --backend cpu|cuda` copies GGUF tensor bytes into
backend-owned tensors and reports a materialized weight table summary.

Fixture CPU output:

```text
materialization status: materialized
model: yvex-tokenizer-test
backend: cpu
tensors_total: 1
tensors_materialized: 1
tensors_failed: 0
bytes_total: 128
bytes_materialized: 128
backend_allocated_bytes: 128
execution_ready: false
reason: graph partial; materialized weights do not imply executable inference
status: weights-materialized
```

CUDA output has the same shape when a CUDA driver/device is available. If CUDA
is unavailable, the command reports `status: weights-unsupported` and exits 5.

M0 materialization does not execute prefill, decode, sampling, graph execution,
or model generation.

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

`yvex plan <path>` builds a graph and estimate-only memory plan. CPU and CUDA
backend capability labels are probed through the backend ABI, but plan output
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

`--backend cuda` reports `available` when the local CUDA driver/device opens,
otherwise `unavailable`. In both cases the plan is not executable:

```text
backend: cuda
backend_status: available|unavailable
execution_ready: false
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

`yvex backend cuda` opens the CUDA backend when the local driver/device is
available. It reports tensor movement and F32 embed capability only:

```text
backend: cuda
status: ready
device: 0
name: <device name>
compute_capability: <major>.<minor>
capabilities:
  tensor_alloc: yes
  tensor_read_write: yes
  op_embed: yes
  op_matmul: no
  op_rms_norm: no
  op_attention: no
status: backend-ready
```

If CUDA is unavailable, it exits 5:

```text
backend: cuda
status: unsupported
reason: <driver/device reason>
status: backend-unsupported
```

## Current `yvex cuda-info`

`yvex cuda-info` probes CUDA through the L0 dynamic CUDA Driver API path. It
does not report matmul, attention, session execution, or inference support.

Available shape:

```text
cuda: available
device_count: >=1

device 0:
  name: <device name>
  compute_capability: <major>.<minor>
  global_memory_bytes: <n>
  free_memory_bytes: <n>
  total_memory_bytes: <n>
  unified_addressing: yes|no
  managed_memory: yes|no

status: cuda-info
```

Unavailable shape:

```text
cuda: unavailable
reason: <driver/device reason>
status: cuda-unavailable
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

`--backend cuda` creates a diagnostic session when CUDA is available; otherwise
it reports the unsupported backend path and exits 5:

```text
backend: cuda
backend_status: unsupported
reason: <driver/device reason>
status: session-backend-unsupported
```

## Current `yvex run`

`yvex run --model FILE --backend cpu --prompt TEXT` opens the engine/backend/
session path, tokenizes the prompt text, accepts those tokens into the session,
and prints an accepted-only runtime result. It does not execute prefill, decode,
sampling, logits, or generation.

J0 observability flags:

```text
--metrics-out FILE
--trace-out FILE
--profile-out FILE
--save-run
--run-dir DIR
```

Plain output:

```text
run status: accepted-only
model: yvex-tokenizer-test
backend: cpu
session_state: partial
prompt_tokens: 3
accepted_tokens: 3
position: 3
execution_ready: false
generation: unsupported
reason: decode runtime is not implemented in I0
```

JSON output is available through `--output json`:

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "run",
  "status": "accepted-only",
  "data": {
    "model": "yvex-tokenizer-test",
    "backend": "cpu",
    "session_state": "partial",
    "prompt_tokens": 3,
    "accepted_tokens": 3,
    "position": 3,
    "execution_ready": false,
    "generation": "unsupported",
    "reason": "decode runtime is not implemented in I0",
    "metrics": {
      "prompt_tokens": 3,
      "accepted_tokens": 3
    }
  },
  "error": null
}
```

`--backend cuda` follows the same accepted-only runtime path when CUDA is
available. If CUDA is unavailable, it reports backend-unsupported and exits 5.

## Current `yvex chat`

`yvex chat --model FILE --backend cpu` starts a line-oriented REPL over the H0
engine/session path. It accepts user text as prompt tokens and prints an
explicit unsupported assistant placeholder.

J0 supports these artifact flags for chat as well:

```text
--metrics-out FILE
--trace-out FILE
--profile-out FILE
--save-run
--run-dir DIR
```

Piped input is deterministic:

```sh
printf "/status\nhello world\n/tokens\n/reset\n/quit\n" | \
  yvex chat --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
```

Output includes:

```text
YVEX chat runtime
session_state: partial
accepted tokens: 3
position: 3
assistant: [generation unsupported in I0]
bye
```

Implemented slash commands:

```text
/help
/status
/model
/backend
/tokens
/reset
/quit
```

## Current `yvexd`

`yvexd` is the K0 server shell. It opens a local HTTP listener and serves
status endpoints only.

```sh
yvexd --host 127.0.0.1 --port 8080
yvexd --host 127.0.0.1 --port 8080 --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
yvexd --host 127.0.0.1 --port 8080 --one-request
```

Implemented endpoints:

```text
GET /health
GET /metrics
GET /v1/models
GET /
```

Generation endpoint requests return HTTP 501 with a `yvex.error.v1` JSON body.

The server shell reports:

```text
generation_available: false
engine_status: partial or not_loaded
backend_status: ready or not_loaded
```

K0 does not implement completion generation, chat-generation endpoint behavior,
streamed generated output, auth, TLS, or multi-client session pooling.

## Current Runtime Metrics And Trace Files

J0 records only implemented runtime-shell work:

```text
engine open
tokenize
accept tokens
chat turns
runtime total
known tensor bytes
unsupported tensor accounting count
```

Metrics JSON uses schema `yvex.metrics.v1`.
Trace JSONL uses schema `yvex.trace.v1`.
Profile JSON uses schema `yvex.profile.v1`.

J0 does not emit decode throughput, TTFT, generated-token counters, CUDA timing,
server metrics, or inference benchmark claims.

## Future Commands

Future command names are not support claims:

```text
yvex bench --model model.gguf --backend cuda --prompt prompts/code.txt --tokens 256
yvex cuda-info
```

Each future command needs source implementation, tests, manual proof, failure
behavior, and documented limitations before appearing in `yvex commands`.

## stdout/stderr

```text
stdout:
  runtime result, JSON envelope, or interactive chat output

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
The user-facing executable surfaces in the current repository are build/bin/yvex
and build/bin/yvexd.
New interface surfaces require an explicit roadmap decision before implementation.
```
