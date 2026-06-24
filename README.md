# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
It sits below chat interfaces, provider APIs, and sampling, at the layer where
a model file becomes inspected structure, tensor metadata, runtime descriptors,
backend allocations, and repeatable evidence.

The project is concerned with the part of local inference that usually
disappears behind a single word like "loaded". In YVEX, a loaded file, a parsed
GGUF, a descriptor, a resident tensor, a live backend, and an executable graph
are different runtime states. The distinction matters because failures hide in
the gaps: tensor names that parse but map to the wrong role, qtypes that are
recognizable as storage but have no compute path, CUDA allocations that prove
memory ownership but not scheduled work, provider endpoints that exist before
the engine behind them can produce logits.

The current repository is therefore lower runtime infrastructure. It opens real
artifacts, reads GGUF metadata and tensor directories, preserves tensor names
and shapes in YVEX descriptors, emits and materializes selected tensors, moves
those tensors through CPU and CUDA residency paths, and records the result
through gates. That is enough to make artifact and backend state concrete
without pretending the transformer graph already runs.

This is also why the CLI is deliberately broad before generation exists. The
commands cover inspection, registry lookup, backend probing, graph planning,
tokenizer and prompt diagnostics, source provenance, native weight inventory,
tensor mapping, selected GGUF emission, qtype policy, quantization job
metadata, imatrix manifests, materialization gates, and daemon status. Those
commands give the future execution path named, testable edges before it is
asked to behave like a chat system.

## Execution boundary

Parsing gives YVEX the artifact's declared structure: GGUF header, metadata,
tensor names, shapes, dtypes, byte offsets, and tokenizer facts where present.
Descriptor construction is the next boundary; it turns file facts into a
runtime view that later commands can reason about. Selected materialization
crosses a different line: the runtime is no longer only describing tensor
bytes, it is moving a chosen tensor into CPU or CUDA-owned storage and forcing
allocation, transfer, cleanup, and error reporting to become explicit.

Backend residency is still not graph execution. A tensor resident on CUDA has
crossed a memory boundary, not a transformer boundary. The missing line is
scheduled work: embedding, normalization, RoPE, attention, routed experts,
logits, KV ownership, decode, and sampling under one runtime path. Until those
pieces exist together, `execution_ready: false` is the honest state.

The binary exposes that state model instead of hiding it. `inspect`,
`metadata`, and `tensors` read artifact structure. `models` gives local names
to external files. `backend` and `cuda-info` report machine facts. `graph` and
`plan` build deterministic planning views. `engine`, `session`, `run`, and
`chat` open diagnostic runtime boundaries for descriptors, backend/session
state, token acceptance, traces, metrics, and profiles. They are useful while
the runtime is being built; they are not generation workflows.

```sh
make
./yvex version
./yvex commands
./yvex info
```

## Live artifact: DeepSeek selected embedding

The live external pressure target is a selected DeepSeek V4 Flash embedding
GGUF documented in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md). The public docs
record artifact identity and tensor facts without publishing a developer
workstation path.

```text
alias: deepseek4-v4-flash-selected-embed
local_path: operator-local, outside repository
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
format: GGUF v3
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU: pass
CUDA: pass
execution_ready: false
```

The selected DeepSeek artifact is deliberately narrow in model scope and still
heavy enough to stress the runtime. It is only one tensor, but it is not toy
data: `token_embd.weight` at `[4096,129280]` in `F16` is about one billion
tensor bytes. That size is large enough to exercise long local artifact names,
checksum identity, GGUF v3 tensor directory layout, shape and dtype
preservation, byte accounting, CPU/CUDA allocation, backend cleanup, and
repeatable gate reporting.

The result is a hard residency checkpoint, not a model run. YVEX can carry this
one real model tensor from operator-local artifact identity through GGUF parse,
descriptor rows, selected materialization, and backend residency. The
transformer graph remains absent. That partial state is the point: it makes the
lower runtime real before the same machinery is extended toward a full
transformer path.

For normal inspection, the top-level path stays short: inspect the artifact,
inspect the tensor table, then materialize the selected tensor on the requested
backend.

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

## Artifact workflow

The repository should be able to travel without the operator's model
directory. Real GGUFs, native safetensors, generated quantization outputs,
local registries, logs, reports, and build artifacts stay on the machine that
owns them. The repo keeps source, public headers, docs, tiny fixtures, tests,
and contracts. That separation is not just cleanliness; it prevents a public
runtime project from becoming a dump of local state.

The local registry exists because real artifact paths are long and
machine-specific. `.yvex/models.local.json` is ignored local state, and
`YVEX_MODELS_REGISTRY` can point commands at another local registry when a
machine needs it. An alias removes path friction, but it does not move the
artifact to a new runtime state. The command that uses the alias still has to
parse, inspect, materialize, or gate the artifact at the requested boundary.

Tiny GGUF fixtures are different from real model artifacts. Fixtures belong in
tests because they validate parser edges, malformed headers, offset handling,
tokenizer metadata, tensor ranks, and layout failures without claiming to be
model inventory. Real model artifacts are external operator assets.

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex inspect local-model
./yvex materialize --model local-model --backend cuda
```

## GGUF intake and tensor mapping

GGUF is the artifact envelope, not the execution engine. Reading it gives YVEX
metadata, tensor directory rows, names, shapes, dtypes, offsets, and tokenizer
metadata where present. Those facts feed descriptors, family mapping, selected
emission, and materialization. They do not become a runnable transformer until
they are connected to scheduled ops, backend kernels, scratch, KV, logits,
decode, and sampling.

YVEX keeps source provenance next to GGUF work because a local runtime needs
more than the final container. The source manifest records where official
weights came from. Native safetensors inventory gives a payload-free view of
source tensors. Tensor mapping is where family-native names become YVEX roles
and proposed GGUF names. Template validation catches structural expectations
before conversion. Selected conversion keeps the scope narrow: one explicit
family, source, tensor, qtype, and output artifact.

```text
official/open weights
  -> source manifest
  -> native safetensors inventory
  -> family tensor mapping
  -> GGUF template / selected emission
  -> selected materialization
  -> future graph execution
```

The command shape follows that lineage. The examples below are deliberately
about inventory, mapping, planning, and selected emission; they are not a
full-model conversion recipe.

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/native/source \
  --status in-progress \
  --out /path/to/source-manifest.json

./yvex native-weights --source /path/to/native/source
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source
./yvex convert plan --arch deepseek4 --native-source /path/to/native/source --out-plan /tmp/yvex-plan.json
./yvex convert emit --arch deepseek4 --native-source /path/to/native/source --tensor model.embed_tokens.weight --target-qtype F16 --out /tmp/yvex-selection.gguf --overwrite
```

## Quantization and qtype boundaries

Quantization in YVEX is a set of separate runtime facts, not a single
"supported" bit. A qtype can be recognized as artifact storage while no backend
kernel computes with it. A policy can allow or reject a qtype for an artifact
shape. A selected emission path can write a requested qtype. An external
quantization job or imatrix manifest can record how an artifact was produced.
Those states are useful only if they stay separate.

This split is practical, not academic. Local model support often breaks when
storage, conversion, calibration, and compute are treated as the same word.
YVEX keeps `qtype-support`, `quant-policy`, `quant-job`, and `imatrix` as
different surfaces so an operator can see whether they are looking at storage
metadata, declared policy, external provenance, calibration evidence, or actual
backend compute coverage.

The current tools make provenance and policy explicit before kernels and graph
execution rely on them. They do not turn external quantization tools into
hidden runtime behavior, and they do not imply that every artifact described by
a manifest can be executed by YVEX.

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

## Backends and machine pressure

The runtime is constrained by the machine before it is constrained by the
README. Tensor bytes have to land somewhere. CUDA allocations have to fail
cleanly. qtypes have to mean different things in storage and compute. Future
KV and scratch budgets have to be sized before prefill and decode become
meaningful. The selected DeepSeek embedding target makes the first part of
that pressure concrete without pretending the rest of the graph exists.

The CPU backend is the reference lane. It gives the project a stable place to
validate parser output, selected materialization, cleanup, and error reporting
before accelerated behavior enters the picture. The CUDA lane is real but
narrow: device discovery, memory accounting, allocation, transfer, device copy,
and limited kernel parity where implemented. That is backend work, not yet a
CUDA transformer backend.

A DeepSeek-class graph has to do much more than place an embedding tensor. It
has to place weights, schedule ops, size scratch, own KV, execute attention and
routed experts, handle quantized matmul, produce logits, decode, and sample.
The current CUDA boundary is valuable because it forces memory ownership and
failure behavior to be precise before those larger kernels and scheduler
decisions arrive.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Runtime and provider boundary

The runtime-facing commands expose engine and session state while execution is
still being built. `engine` opens descriptor/tokenizer/graph diagnostics.
`session` creates lifecycle state over an engine and backend. `run` accepts one
prompt through the diagnostic path and reports accepted-only state. `chat` is
the console surface and future REPL direction. Those commands are useful
because they expose runtime edges without pretending to produce model text.

`./yvexd` plays the same role at the provider boundary. It keeps a daemon shape
available for health, metrics, and model listing while generation remains
outside the daemon until an executable graph exists behind it. The implemented
status endpoints are useful process surfaces, not generation endpoints.

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

The current status endpoints are `GET /health`, `GET /metrics`, and
`GET /v1/models`. They should be read as provider-shell structure, not as model
serving.

## Validation

Validation ties README claims to command behavior. `make check` runs the
baseline C test surface, smoke tests exercise the CLI and daemon paths, docs
surface tests protect the public documentation boundary, and repository surface
guards keep the minimal layout from drifting. CUDA validation is explicit
because it depends on a CUDA-capable host.

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

CUDA-capable hosts:

```sh
make check-cuda
```

Artifact guardrails keep model weights and generated artifacts out of git:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is no committed real model weights, no generated model
GGUFs, no local registries, no reports/logs/build outputs, and no benchmark
artifacts. Tracked GGUF files are tiny parser fixtures under `tests/`.

## Source layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             provider daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA host bridge and kernel unit
gguf/               GGUF parser, conversion, family mapping, quant policy
include/yvex/       public C API
tests/              compact runners, fixtures, and vectors
docs/               API, contract, internal spine
```

## Documentation

`AGENTS.md` defines operating rules for humans and coding agents.
`MODEL_ARTIFACTS.md` is the external artifact card. `docs/api.md` describes the
public C API, `docs/contract.md` defines runtime/backend/filesystem/CLI
contracts, and the internal roadmap stays in `docs/spine.md`.

## License

YVEX is licensed under the MIT license.
