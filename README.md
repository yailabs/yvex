# YVEX

Vector Exec: C inference engine for local models. CUDA / Metal.

YVEX is a native C inference engine for local open-weight models.

The project is developed today on DGX Spark for the CUDA path and on MacBook for
the first Metal path with smaller local models. The engine is still below full
text generation. Its current work is the lower runtime path: taking model
artifacts apart, turning tensor facts into runtime descriptors, materializing
selected weights, attaching resident buffers to engine state, and executing the
first graph slices over those bytes.

The point is not to mark a model as loaded and move on. YVEX keeps the path
inspectable from artifact to execution, so the runtime can explain which file
was accepted, which tensor ranges were understood, which buffers became
resident, which graph operation consumed them, which report proves that handoff,
and where execution stops before the next stage is available.


## Status

YVEX currently owns the lower part of the local inference path.

GGUF parsing and structural validation are active. The runtime can read the
artifact structure, inspect tensor metadata, build selected tensor descriptors,
check integrity before execution, and resolve local model aliases through the
registry. Descriptor construction is the important turn: tensor name, dtype,
rank, shape, file offset, absolute byte range, alignment, selected-token slice,
and graph role become a runtime fact before materialization is allowed.

The first real graph slices are already executable. The embedding path reads
from `token_embd.weight` by validating a token id, narrowing the admitted tensor
range to the selected row, moving that row through the backend path, and
checking output against an independent artifact read. The
embedding-plus-RMSNorm path adds the first-layer normalization weight, currently
represented by tensors such as `blk.0.attn_norm.weight`. Output is checked
against independent reference reads from the same artifact, so the graph path is
not only producing bytes internally but validating range math, descriptor roles,
backend residency, graph dispatch, cleanup, and reference comparison against the
source artifact.

Token input is explicit and bounded. Prefill-state summaries already record
sequence-level execution over the selected segment. KV ownership is present as
diagnostic session state with shape, capacity, append, read, clear, and overflow
behavior. That KV path is useful because it gives the runtime a real owned
state boundary, but it is not yet the attention-backed decode state used by a
complete transformer generation loop.

Full transformer execution is still ahead. YVEX does not yet claim full-model
prefill, real attention-backed KV, decode, output-head logits, vocabulary
sampling, streaming generation, server-backed generation, broad qtype compute
support, or Metal performance numbers. Storage recognition, materialization
support, backend compute support, graph support, and generation support remain
separate claims.


## Execution chain

```text
source weights
  -> source manifest
  -> native tensor inventory
  -> family tensor map
  -> GGUF artifact
  -> integrity / admission
  -> runtime descriptors
  -> materialization
  -> engine-owned state
  -> graph execution
  -> token input
  -> prefill state
  -> KV ownership
  -> decode
  -> logits
  -> sampling
  -> generation
  -> server surface
```

This is the architecture of the runtime path, not a release checklist. The
current implementation is active through selected graph execution, token input,
prefill summaries, and diagnostic KV ownership. Decode, logits, sampling,
generation, and provider/server-backed generation arrive only after the same
lower path can support them.

## Machines and backends

YVEX separates machine reality from backend claims. CPU remains the reference
lane for parser behavior, fixtures, integrity reports, diagnostics, and small
graph correctness. CUDA is the active accelerated path and is being developed
on DGX Spark. Metal is being started on MacBook with smaller local models
before larger Apple unified-memory targets become relevant.

| Lane              | Machine reality                      | Runtime role                                                 | Claim        |
| ----------------- | ------------------------------------ | ------------------------------------------------------------ | ------------ |
| CPU               | developer host                       | reference execution, fixtures, diagnostics, integrity checks | current      |
| CUDA              | DGX Spark                            | active acceleration path and pressure lane                   | current      |
| Metal             | MacBook                              | initial backend work with smaller local models               | experimental |
| Metal high-memory | larger Apple unified-memory machines | future larger/full local model target                        | future       |
| CUDA scale        | additional CUDA systems              | future memory pressure and throughput work                   | future       |

Generation throughput does not belong in this table yet. A machine can be a
pressure target before generation exists. A backend can allocate before it can
consume a graph. Backend posture and benchmark evidence are different surfaces:
this table says where the engine can or will run; benchmark tables later must
separate machine, backend, artifact, storage/quantization, prompt shape,
prefill, and generation.

## Artifacts and model path

Real model weights stay outside git. The repository carries code, public
headers, documentation, tiny fixtures, and tests; larger model artifacts remain
operator-local.

A local inference engine cannot treat `GGUF` as a file extension only. YVEX
treats an artifact as the result of a traceable construction path, not as a file
extension. Source weights produce manifests and native tensor inventories.
Family maps connect model-native tensor names to runtime roles. Conversion
policy decides which tensor subset and storage format are emitted. GGUF becomes
useful to the engine when its architecture metadata, tensor directory, storage
types, offsets, byte ranges, and alignment can be checked and turned into
descriptors that later runtime phases are allowed to trust.

Selected artifacts sit between tiny fixtures and future full-model GGUFs. They
are large enough to exercise real range accounting, transfer behavior, backend
residency, cleanup, digest checks, and reference extraction, but narrow enough
to keep the current executable path honest. A selected embedding artifact proves
the first real tensor path; an embedding-plus-RMSNorm artifact proves the first
real multi-op segment. Neither one is a claim of general GGUF execution or
complete model generation.

Quantization stays separated from runtime claims. Recognizing a storage type
does not imply graph compute support. Emitting a selected GGUF does not imply
full-model support. Calibration or imatrix evidence records artifact
construction facts, but inference begins only when the runtime can materialize
the required tensors and execute the graph path that consumes them.

## Runtime surface

The CLI exposes the lower runtime boundaries directly. Higher-level operator
flows can compress these commands later, but the boundaries stay separate
because each one answers a different technical question.

Runtime descriptors are the first important boundary after GGUF parsing. They
carry the tensor role, shape, storage information, file offset, byte range,
alignment, selected-token range, and graph use that the engine needs before
materialization. Once a descriptor exists, the tensor is no longer just a GGUF
directory entry; it has become a checked runtime fact.

Path and target commands define where operator-local artifacts are expected to
live and which model targets are known to the local setup.

```sh
make
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

Registry and integrity commands decide whether a named local artifact can be
trusted for the next runtime phase.

```sh
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed

./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
```

Materialization and engine attachment move from file facts into resident runtime
state.

```sh
./yvex materialize \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu

./yvex engine \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu
```

Graph, prefill, and KV commands exercise the current executable path.

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --execute-segment \
  --segment embedding-rmsnorm \
  --tokens 0,1 \
  --token-index 0

./yvex prefill \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --segment embedding-rmsnorm \
  --tokens 0,1,2

./yvex kv \
  --layers 1 \
  --heads 2 \
  --head-dim 4 \
  --capacity 8 \
  --append-demo \
  --read-position 0
```

`yvexd` remains a status, diagnostics, and server-boundary surface until the
lower engine path reaches KV-backed prefill, decode, logits, sampling,
streaming, and failure reporting.

## Why C

YVEX uses C because the engine sits at the point where file geometry becomes
runtime state.

GGUF access in this project is not an opaque load operation. The runtime works
with tensor directories, offsets, alignment, byte counts, qtype storage width,
and selected tensor ranges. Those values decide whether a descriptor is valid,
whether a tensor can be materialized, and whether a graph operation is allowed
to consume the resulting buffer. C keeps that arithmetic visible at the boundary
where wrong range math would become unsafe execution.

The main runtime objects are descriptors, artifact identity records,
materialization records, backend buffers, engine state, sessions, graph outputs,
prefill summaries, and KV storage. Their lifetime matters because the engine
needs to know which bytes came from the artifact, which memory became resident,
which operation consumed it, and which state survived afterward. This is not
only about manual memory management; it is about keeping ownership aligned with
the execution phases.

The backend boundary also needs to stay explicit. CPU, CUDA, and Metal are
separate runtime lanes, not interchangeable labels. CPU gives the reference
path and diagnostic behavior. CUDA gives device residency and accelerated
dispatch where implemented. Metal will follow as its own backend path. Backend
residency does not imply graph support, and graph support does not imply
generation.

The current kernel path starts with embedding lookup and RMSNorm. Later
primitive families can add projection/matmul, RoPE, attention, MLP, logits, and
sampling, but each one has to enter through the same discipline: selected bytes
become resident buffers, a graph operation declares what it consumes, the
backend dispatch runs the operation, and the output can be checked or reported.
The C layer is where that route stays explicit.

## Integrity and admission

YVEX treats integrity as part of runtime execution, not as a detached validation
pass.

Admission checks stop the engine before the next phase consumes facts that have
not been proven. GGUF structure, tensor directory consistency, tensor names,
shape accounting, storage recognition, byte-count math, offsets, alignment,
file identity, registry drift, selected tensor readiness, token bounds, backend
support, and graph preflight all belong to the same boundary.

A failure should identify where execution stopped and which fact was missing or
invalid. Parser failures, integrity failures, range failures, materialization
failures, backend dispatch failures, graph failures, reference-read failures,
session failures, and cleanup failures need to remain distinguishable because
they point to different parts of the engine.

## Benchmarks and evaluation

YVEX publishes speed numbers only for runtime paths that exist.

Prefill and generation are separate metrics because they stress different parts
of the engine. Fixture correctness is not model quality. Selected graph-slice
correctness is not text generation. Capability evaluation belongs after real
generation runs through the same decode, logits, and sampling path used by
operators.

| Machine                   | Backend | Model / artifact               | Quant / storage          | Prompt / context   | Prefill         | Generation | Status        |
| ------------------------- | ------- | ------------------------------ | ------------------------ | ------------------ | --------------- | ---------- | ------------- |
| DGX Spark                 | CUDA    | selected embed + RMSNorm       | current selected storage | diagnostic tokens  | command-visible | N/A        | current slice |
| MacBook                   | Metal   | small local artifact / fixture | TBD                      | TBD                | TBD             | N/A        | experimental  |
| High-memory Apple Silicon | Metal   | full local model               | TBD                      | long prompt        | future          | future     | future        |
| Additional CUDA host      | CUDA    | larger local model             | TBD                      | medium/long prompt | future          | future     | future        |

Generation remains `N/A` until decode, logits, and sampling exist as executable
runtime stages.

## Build and validation

```sh
make
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
make check-cuda
```

## Repository policy

Real model weights stay outside git. Operator-local paths stay local. Tiny
fixtures can live under tests. Public claims must match executable runtime
boundaries.

Storage support, compute support, backend support, and generation support are
separate claims.

## Documentation

`docs/operator-runbook.md` carries the command-first workflow. `docs/api.md`
describes the public C surface. `docs/contract.md` defines runtime behavior and
public claims. `MODEL_ARTIFACTS.md` records artifact posture. `AGENTS.md`
defines repository operating rules. `docs/spine.md` remains the internal
delivery map.

## Acknowledgements

YVEX lives in the open-weight local inference ecosystem shaped by GGUF, GGML,
llama.cpp, CUDA tooling, and native model execution work.

## License

MIT.
