# YVEX

YVEX is a native C inference engine for local open-weight models. It is
**intentionally narrow**: not a generic GGUF loader, not a universal runtime,
and not a vague local-AI wrapper. Its job is to turn concrete model artifacts
into **owned, inspectable runtime state** across parsing, descriptor
construction, backend residency, engine ownership, graph execution, and session
state.

A GGUF file can parse cleanly while tensor ranges are still wrong. A tensor can
sit in backend memory while no graph path can actually consume it. A daemon can
answer requests while generation remains a promise. YVEX keeps those states
separate and reportable. That is **not a loaded flag**.

The current path starts from real artifacts. It parses GGUF structure, builds
descriptors for selected tensors, moves chosen DeepSeek-class `F16` weights onto
CPU or CUDA, attaches them to engine-owned state, and runs the first graph
slices over those bytes. The first slice is token embedding lookup over
`token_embd.weight`. The next adds RMSNorm using a real first-layer weight such
as `blk.0.attn_norm.weight`. Both slices are checked against independent
reference data read directly from the artifact. Session state now includes a
small bounded KV store with explicit shape, capacity, append, read, clear, byte
accounting, and overflow behavior. This is still diagnostic KV, not the final
attention-backed decode state.

The next meaningful runtime step is a transformer block that stays on the same
owned path: token input, embedding, normalization, attention, MLP, residual
state, and logits. Generation only becomes useful after that path is executable
inside the engine.

C is the right language here because this layer is mostly about ownership.
Memory must belong to someone. Tensor ranges must be explicit. Cleanup must be
a defined phase. Backend residency must be reported separately from graph
support. When a report says something ran, it must be clear which bytes moved,
which graph consumed them, and which state survived afterward.

## Why this project exists

Open-weight inference has moved past the point where simply running a model
locally is interesting. That bar is now low enough to be ordinary. The harder
problem is running models large enough to matter while keeping the whole
machine path understandable and debuggable.

Small dense models hide a lot of bad decisions. They fit in RAM easily, move
quickly, and tolerate sloppy accounting, vague state ownership, and loose
backend boundaries. Larger open-weight models do not forgive those things.
Quantization choices start to affect behavior, not just file size. KV state
becomes one of the dominant memory objects. Tensor placement and backend kernel
coverage turn into first-class architectural decisions. Long prefill and
token-by-token decode stress different parts of the system. When something goes
wrong, you need to know where in the chain the failure happened.

YVEX is built for that pressure. It grows through narrow, executable slices
rather than jumping from parser straight to generation. Tiny deterministic
fixtures keep parser, corruption handling, executor, and backend behavior
exactly checkable. Selected real-tensor artifacts, starting with large
embedding weights and then embedding plus RMSNorm, put model-scale pressure on
range math, backend transfer, allocation limits, digest checks, cleanup, and
reference comparison. Multi-tensor graph segments then add scheduling,
intermediate memory planning, and another place where execution can be wrong
before the full transformer path exists.

Model-building tooling lives close to the runtime for the same reason. Once
weights have been downloaded, inventoried, mapped, quantized, emitted as GGUF,
registered, and materialized, the engine needs a traceable record of what
happened. Source manifests, native tensor inventories, family maps, qtype
policy, GGUF template validation, integrity reports, and artifact cards are not
paperwork around the engine. They are the chain that lets the runtime explain
why a local artifact should be trusted and what it actually contains.

Speed numbers and capability tests only become meaningful after that chain
exists. Without a clear path from source weights to executable state, a
benchmark is just a number attached to an opaque file.

## Status

YVEX is still below full text generation.

**Current runtime path:**

- Parse GGUF artifacts and perform structural validation.
- Build descriptors for selected tensors.
- Materialize selected DeepSeek-class `F16` weights on CPU or CUDA.
- Attach resident weights to engine-owned state.
- Run graph slices for token embedding lookup and embedding plus RMSNorm, with
  independent reference checks.
- Own a small bounded KV store with explicit lifecycle and byte accounting.

The next runtime block is transformer execution on the same artifact-to-engine
path. Full-model prefill, decode, logits, sampling, streaming generation, and
server-backed generation are still ahead.

The project is early. Use tracing and detailed logs when behavior looks off,
and include full context when reporting issues.

## Runtime architecture

YVEX follows one continuous path from source evidence to local execution. Each
step turns file facts into runtime facts that the next step can depend on.

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

The diagram is architecture, not a release checklist. Current implementation
status lives in the Status section above.

## Model artifacts

Real model weights stay outside git. The repository carries code, public
headers, documentation, tiny fixtures, and tests. Large artifacts remain
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
download, create, or register anything.

A typical selected-artifact registration:

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
is the result of a traceable chain, not just a file with the right extension.
A local inference engine cannot treat `GGUF` as a file extension only.

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

Low-level source-to-selected commands:

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

Quantization and imatrix work follow the same separation: a qtype can be valid
for storage before the executor has compute support for it. Manifests record
the work without turning it into a runtime claim.

## Backends and machines

Local inference is a machine problem. Memory size, storage speed, backend
ownership, and kernel coverage decide what the runtime can actually prove.

| Machine class | Backend | Role in current engine | Notes |
| --- | --- | --- | --- |
| CPU-only developer machine | CPU | Reference lane for parser, descriptors, token input, integrity, cleanup, KV tests, and small graph correctness. | Always available for diagnostics. |
| CUDA workstation | CUDA | Acceleration path for selected tensor materialization, fixture parity, embedding, and embedding plus RMSNorm slices. | Primary development target. |
| DGX Spark / GB10-class | CUDA | Pressure target for larger artifacts, memory behavior, and allocation limits. | Future full-model stress testing. |
| High-memory MacBook / Mac Studio | Metal | Planned unified-memory lane. | Important for personal high-end machines. |
| Strix Halo-class | ROCm | Planned AMD unified-memory lane. | Same unified-memory pressure class. |

YVEX currently claims CPU and CUDA only for the surfaces that exist today.

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

Controlled block fixture, an exact F32 world:

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
