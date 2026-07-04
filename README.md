# YVEX

YVEX is a native C inference engine for local open-weight models. It is built
around turning real artifacts into owned runtime state that can be explained at
each step instead of hidden behind a loaded flag. The engine parses GGUF
structure, builds descriptors for selected tensors, moves chosen weights onto
CPU or CUDA, attaches them to engine-owned state and runs the first graph slices
over those bytes. Right now that means embedding lookup over a real
`token_embd.weight` followed by RMSNorm using an actual first-layer weight such
as `blk.0.attn_norm.weight`, with output checked against independent reference
reads from the same artifact. A small bounded KV store is already there with
explicit shape, capacity, append, read, clear and overflow behavior. This is
still diagnostic KV, not the final attention-backed decode state. Full
transformer blocks, decode, logits and generation are still ahead.

The project targets the machines where these models actually become useful:
high-memory workstations, MacBooks and Mac Studios with unified memory, and
CUDA systems like DGX Spark class hardware. CPU is the reference lane for
correctness and diagnostics. CUDA is the current acceleration path. Metal is
the next target because unified memory and fast local storage change what is
practical on personal hardware for this size of model. The engine stays narrow
on purpose. It works with selected artifacts that match the current path rather
than pretending to be a general GGUF runner.

The pipeline is kept visible so an operator can follow it from source to
execution. Tensors can be pulled from Hugging Face or GitHub, manifests and
inventories built, quantization and tensor selection decided according to the
specific machine and model family, a matching GGUF emitted, and the result
materialized and attached without losing the chain. GGUF is used because it
supplies concrete facts - architecture metadata, tensor directory, storage
formats, offsets and alignment - that let the runtime build real descriptors
instead of guessing ranges or layouts. Those descriptors then drive
materialization and the graph work that follows.

C is the right language here because this layer is mostly about ownership.
Memory must belong to someone. Tensor ranges must be explicit. Cleanup must be
a defined phase. Backend residency must be reported separately from graph
support. When a report says something ran, it must be clear which bytes moved,
which graph consumed them, and which state survived afterward.

## Why this project exists

Open-weight inference has moved past the point where simply running a model
locally is interesting. That bar is low enough now to be ordinary. The harder
problem is running models large enough to matter while keeping the whole
machine path understandable and debuggable as things get bigger.

Small dense models hide a lot of bad decisions. They fit in RAM easily, move
fast and tolerate sloppy accounting, vague state ownership and loose backend
boundaries. Larger open-weight models do not forgive those things. Quantization
starts affecting real behavior, not just file size. KV state becomes one of the
main memory objects. Tensor placement and backend kernel coverage turn into
architectural decisions. Long prefill and token-by-token decode stress
different parts of the system. When something goes wrong you need to know
exactly where in the chain it broke.

YVEX grows through narrow executable slices instead of jumping from parser
straight to generation. Tiny fixtures keep parser, corruption handling,
executor and backend behavior exactly checkable. Selected real-tensor artifacts
put model-scale pressure on range math, transfer, allocation, digest checks and
reference comparison before the full transformer path exists. Multi-tensor
segments then add scheduling and intermediate memory without pretending the
whole model is already there.

Model-building tooling lives close to the runtime for the same reason. Once
weights have been downloaded, inventoried, mapped, quantized and emitted as
GGUF, the engine needs the record of what happened so it can explain later why
a particular local artifact behaves the way it does. Source manifests, native
inventories, family maps, qtype policy and integrity reports are not
decoration. They are the chain that keeps the route from open weights to
executable state traceable.

Speed numbers and capability tests only become meaningful after that chain
exists. Without a clear path from source weights to executable state, a
benchmark is just a number attached to an opaque file. The project stays
opportunistic. Right now the selected DeepSeek-class artifacts are the right
pressure targets because of size, capabilities and how well they survive the
current quantization and materialization path. If a better open-weight model
appears for the same memory class, the focus can shift. The constraint stays
the same: credible local inference on high-end personal machines with real
ownership and transparency from start to finish.

## Status

YVEX is still below full text generation. The current runtime can parse GGUF
artifacts and run structural validation, build descriptors for selected
tensors, materialize chosen DeepSeek-class `F16` weights on CPU or CUDA, attach
the resident weights to engine-owned state, and run the first graph slices.
Those slices cover token embedding lookup and embedding plus RMSNorm using a
real first-layer weight, with output checked against independent reference data
read from the artifact. A small bounded KV store is already present with
explicit shape, capacity, append, read, clear, close and byte accounting. This
is diagnostic KV, not the final attention-backed decode state.

The next block on the same path is transformer execution. Full-model prefill,
decode, logits, sampling, streaming generation and server-backed generation are
still ahead. The project is early. Use tracing and detailed logs when something
looks off, and include full context when reporting issues.

## Runtime architecture

YVEX follows one continuous path from source evidence to local execution. Each
step turns file facts into runtime facts the next step can depend on.

```text
source weights
  -> source manifest
  -> native tensor inventory
  -> family tensor map
  -> GGUF artifact
  -> artifact integrity
  -> runtime descriptors
  -> backend residency
  -> engine-owned state
  -> graph execution
  -> token input
  -> prefill state
  -> KV ownership
  -> KV-backed prefill
  -> decode
  -> logits
  -> sampling
  -> generation
  -> server and agent surfaces
```

The diagram is the architecture, not a release checklist. Current
implementation status lives in the Status section.

## Model artifacts

Real model weights stay outside git. The repository carries code, public
headers, documentation, tiny fixtures and tests. Large artifacts remain
operator-local.

YVEX currently works with selected pressure artifacts that sit between tiny
fixtures and future full-model GGUFs. Their job is to force real runtime
behavior at model scale before attention, MLP, logits, and sampling are
implemented.

Start by making the local layout explicit:

```sh
make
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

These commands only report the operator-local storage layout. They do not
download, create or register anything.

A typical selected-artifact registration looks like this:

```sh
./yvex models add \
  --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" \
  --alias deepseek4-v4-flash-selected-embed \
  --support-level selected-tensor-materialized

./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
```

The alias gives a stable local handle. YVEX continues to check file identity,
metadata drift, tensor readiness, and graph admission behind it.

## Model-building path

YVEX treats model-building as part of the engine path. A useful local artifact
is the result of a traceable chain, not just a file with the right extension. A
local inference engine cannot treat `GGUF` as a file extension only.

```text
official/open weights
  -> source manifest
  -> native tensor inventory
  -> family tensor mapping
  -> GGUF template validation
  -> selected emission
  -> local registry alias
  -> integrity report
  -> materialization
  -> engine attachment
  -> graph execution
```

Low-level source-to-selected commands look like this:

```sh
./yvex source-manifest create \
  --hf-repo "deepseek-ai/DeepSeek-V4-Flash" \
  --revision "main" \
  --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --status in-progress \
  --out "$HOME/lab/models/gguf/deepseek/deepseek-source-manifest.json"

./yvex native-weights --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20

./yvex tensor-map --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20

./yvex convert plan \
  --arch deepseek4 \
  --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --out-plan "$HOME/lab/models/gguf/deepseek/deepseek-selected-plan.json"

./yvex convert emit \
  --arch deepseek4 \
  --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  --tensor embed.weight \
  --target-qtype F16 \
  --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" \
  --overwrite
```

Quantization and imatrix work follow the same separation. A qtype can be valid
for storage before the executor has compute support for it. Manifests record
the work without turning it into a runtime claim.

## Backends and machines

Local inference is a machine problem. Memory size, storage speed, backend
ownership and kernel coverage decide what the runtime can actually prove and
measure.

The table below shows the current and planned surfaces the way they will be
benchmarked later: by machine class, backend, artifact type, workload and
separate prefill versus generation numbers. Generation numbers stay N/A until
the decode, logits and sampling path exists. No throughput number is implied by
this table.

| Machine | Backend | Artifact / Slice | Workload | Prefill / Slice | Generation |
| --- | --- | --- | --- | --- | --- |
| CPU developer machine | CPU | fixtures + selected embed | partial token / segment | command-visible | N/A |
| CPU developer machine | CPU | embed + RMSNorm | prefill foundation | command-visible | N/A |
| CUDA workstation | CUDA | selected embed | partial token | command-visible | N/A |
| CUDA workstation | CUDA | embed + RMSNorm | prefill foundation | command-visible | N/A |
| CUDA workstation | CUDA | embed + RMSNorm + KV | KV-backed prefill | command-visible | N/A |
| DGX Spark / GB10-class | CUDA | larger selected artifacts | memory pressure workloads | future pressure row | N/A |
| High-memory MacBook / Mac Studio | Metal | future selected GGUF | long prompts | future row | future row |
| Strix Halo-class | ROCm | future selected GGUF | long prompts | future row | future row |

YVEX currently claims CPU and CUDA only for the surfaces that exist today.
Metal and ROCm numbers will appear when those backends land and real artifacts
are measured on them. Generation stays N/A across the board until the full
decode path is executable on the same owned chain.

## Larger-model direction

YVEX is aimed at models large enough that local inference becomes real
infrastructure rather than a demo.

On these machines a larger model changes the shape of the runtime. It may only
fit under a specific quantization profile. KV state becomes a first-order memory
citizen. Scratch space competes with resident weights. Backend placement
becomes part of the execution plan. Long-context prefill stresses different
parts of the system than autoregressive decode. Distributed execution, when it
arrives, will mostly be a capacity and long-prefill problem before it becomes a
comfortable generation path.

YVEX prepares for that future by keeping placement, ownership, and measurement
explicit at every layer. Full-model materialization is not the same as
selected-tensor materialization. KV ownership is not the same as prefill
summaries. Decode is not logits. Sampling is not generation. Each boundary earns
its own report.

For larger source-tensor targets start with:

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

## Current runtime surface

YVEX exposes low-level commands because they cross real runtime boundaries. A
future higher-level UX can compress common flows, but the evidence must remain
available.

Short selected-artifact path:

```sh
make
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

Embedding plus RMSNorm segment:

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu \
  --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
```

Controlled block fixture (exact F32 world):

```sh
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
```

Prefill foundation:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2
```

KV-backed prefill binding:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

Standalone KV proof:

```sh
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

The operator runbook contains longer transcripts, CUDA variants, expected output
fields, and failure-mode examples.

## Integrity and admission

Integrity checks happen before materialization and graph execution. They verify
the facts the next stage depends on: GGUF structure, tensor directory
consistency, name uniqueness, shape accounting, dtype recognition, byte-count
math, tensor ranges, selected token slices, digest identity, registry metadata
drift, and graph preflight.

Normal report:

```sh
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
```

Admission stops the run at the first failing phase. Structural problems stop
before materialization. Digest drift stops before alias assumptions are trusted.
Token range errors stop before reference extraction. The engine fails with
evidence.

## Speed and evaluation

YVEX will only publish speed numbers for runtime paths that actually exist. A
useful row must name the machine, backend, artifact, qtype, prompt shape,
context length, command, and reproducibility conditions. Prefill and generation
are measured separately because they are different workloads.

Evaluation follows the same rule. Fixture correctness is not capability
evaluation. Selected graph checks are not model quality. KV ownership tests are
not prefill quality. Logits regression and capability regression become
meaningful only after the real generation path exists.

## Server and operator surface

`yvexd` is the current daemon boundary. It reports health, metrics, model
listing, model references, and generation status. Provider-backed generation,
streaming, and agent surfaces will arrive once KV-backed prefill, decode,
logits, and sampling are solid. Until then the daemon remains a status and
diagnostics surface.

The CLI stays explicit. Low-level commands stay visible because they cross
different runtime boundaries.

## Build and validation

```sh
make
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

CUDA hosts should also run:

```sh
make check-cuda
```

Artifact guardrail:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Real model weights stay outside git. Tracked GGUF files are only tiny fixtures
under `tests/`.

## Documentation

- `docs/operator-runbook.md` - command-first workflows for artifacts, integrity,
  materialization, graph proofs, token input, prefill, KV, daemon status, and
  validation.
- `docs/api.md` - public C surface, ownership rules, report structs, backend
  capabilities, token input, prefill summaries, KV state, materialization
  summaries, graph results, and integrity surfaces.
- `docs/contract.md` - runtime behavior for CLI output, filesystem state,
  registry resolution, backend boundaries, server status, validation, and public
  claims.
- `MODEL_ARTIFACTS.md` - selected pressure artifacts and validation posture.
- `AGENTS.md` - repository operating rules.
- `docs/spine.md` - internal delivery map, not product documentation.

## Acknowledgements

YVEX lives in the open-weight local inference ecosystem shaped by GGUF, GGML,
llama.cpp, CUDA tooling, and the people who kept native model execution
practical. The project is separate but learns from that engineering lineage.

## License

YVEX is licensed under the MIT license.
