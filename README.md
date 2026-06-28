# YVEX

YVEX is a native C inference engine for local open-weight models.

It sits below chat interfaces, provider APIs, and sampling. Its job is not to
wrap another runtime or to make a provider-shaped shell look alive before the
engine exists. Its job is to turn local model files into owned runtime state:
model facts, tensor roles, backend-resident weights, engine/session ownership,
scheduled graph work, and eventually token production.

That "eventually" matters. YVEX does not yet run a complete text-generation
path. What it has today is narrower and real: controlled fixture graph
execution, selected `F16` embedding execution over real model bytes, a selected
embedding-plus-RMSNorm segment over multiple real tensors, and explicit token
sequences routed into those bounded paths. The current work is below prefill,
`KV`, decode, logits, sampling, and generation, but it is already past pure
inspection.

The GGUF, registry, integrity, model-building, and qtype machinery exists
because local inference needs those things close to the engine. A runtime that
cannot explain tensor layout, model identity, memory ownership, graph inputs,
and failure points is easy to demo and hard to trust.

## Why This Exists

Local inference is easy to make theatrical. A prompt box can hide a weak
runtime. A server can answer `/v1/models` while no lower path can produce
logits. A model can be "loaded" in half a dozen incompatible senses: the file
exists, the header parsed, metadata was read, tensors were named, weights moved
to a device, graph work was scheduled, or tokens actually came out.

YVEX is for the space between those words. It should be possible to ask where a
local model stopped and get a precise answer: file identity, tensor mapping,
shape accounting, backend allocation, graph preflight, dispatch, reference
comparison, token input, future `KV`, future logits. That precision is not
ceremony. It is what lets a native engine grow without dragging a fog of
accidental claims behind it.

The artifact work is therefore not a detour. A local inference engine has to
know what the model file says, which tensors mean what, how many bytes are safe
to read, which qtypes are storage-only, which buffers belong to the backend, and
which session can observe or own state. If those facts are not solid, a later
prompt interface is just theatre with a nicer command line.

## The Local Inference Bet

YVEX is betting that local inference will matter again because open-weight
models and high-memory local machines are becoming good enough to justify
serious native runtime work. DeepSeek-class models are useful pressure targets
because they force real choices about tensor layout, qtype boundaries, memory
movement, backend behavior, and runtime ownership. They are not branding.

The target family can change. If another open-weight model becomes the better
local runtime target, YVEX should move. Support has to be earned through tensor
mapping, artifact identity, backend residency, graph execution, and tests, not
declared because a filename looks familiar.

The machine story is deliberately split into lanes. `CPU` is the reference lane
for correctness and diagnostics. `CUDA` on Linux is the current accelerated
lane, including selected materialization and the narrow graph paths that exist
today. Metal on macOS and ROCm on Strix Halo-class machines are important future
local-machine lanes, not current support claims. DGX Spark / GB10 is a
CUDA-class pressure target for larger local runtime work.

## What Runs Today

YVEX has two proof lanes.

One lane is controlled and deterministic. Tiny `F32` GGUF fixtures prove the
executor and backend mechanics: attached tensor data is read, an embed node is
dispatched, output is allocated, and values can be checked exactly on `CPU` and
on `CUDA` hosts.

The other lane is real but narrow. Selected DeepSeek-class tensors stay
operator-local, pass identity and integrity checks, materialize into backend
storage, attach to engine-owned state, and participate in scheduled graph work.
The first real segment reads `token_embd.weight` and produces an `F32` embedding
vector with an independent raw-artifact reference comparison. The larger segment
adds the first RMSNorm weight and checks the final vector by checksum and
max-diff. Explicit token lists can select which token enters the fixture,
embedding, or embedding-plus-RMSNorm path.

That is meaningful engine work, not a complete transformer. There is no
prompt-backed prefill, no session-owned `KV` runtime, no decode step, no logits
buffer, no sampler, and no generated text.

The command details live in [docs/operator-runbook.md](docs/operator-runbook.md).
The short shape is:

```sh
make
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
```

And the bounded graph proof is explicit:

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

That graph command reports the selected tensor, backend, output size,
checksum/reference checksum, max diff, guard status, and false readiness for
full execution. The runbook keeps the longer CPU/CUDA examples, expected output,
fixture proof, segment proof, and failure modes.

## Model Intake and Artifact Discipline

A local inference engine cannot treat `GGUF` as a file extension only. The
engine needs the container because it carries tensor names, shapes, dtypes,
offsets, qtypes, tokenizer metadata, and model-family facts. YVEX keeps those
facts close to the runtime because graph execution and backend memory depend on
them.

Real model artifacts stay outside the repository. The repo contains source,
headers, docs, tiny fixtures, and tests; operator machines hold native weights,
generated GGUFs, registries, reports, logs, and build output. Public docs record
artifact identity and tensor facts without publishing personal filesystem paths.

The active pressure artifact is the selected DeepSeek embedding GGUF:
`token_embd.weight`, shape `[4096,129280]`, `F16`, about one billion tensor
bytes, with file identity recorded in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md).
That single tensor is enough to make memory ownership, digest checks, shape
accounting, backend allocation, cleanup, engine attachment, and graph dispatch
concrete. It is not enough to claim the model runs.

The model-building tools support that engine path. `source-manifest`,
`native-weights`, `tensor-map`, `gguf-template`, and selected `convert` commands
make provenance, native tensor inventory, family mapping, template expectations,
and narrow emission visible. `qtype-support`, `quant-policy`, `quant-job`, and
`imatrix` keep storage, policy, external quantization work, calibration
evidence, and backend compute coverage separate. They are infrastructure for a
runtime, not hidden inference.

Artifact integrity is the refusal layer before runtime-owned state. Structural
parse checks, tensor shape/range accounting, digest identity, registry metadata
drift checks, materialization preflight, and graph-entry guards must pass before
allocation, transfer, dispatch, or raw reference reads. These are local
correctness and corruption-safety checks. They are not supply-chain security,
malware detection, model-quality validation, or provenance trust.

## Boundaries

| YVEX is not | Boundary |
| --- | --- |
| a generic GGUF runner | container parsing is engine intake, not arbitrary model execution |
| a chat UI | `chat` is diagnostic until the engine can produce model output |
| a provider generation server | `yvexd` wires status/provider shape before generation exists |
| a benchmark leaderboard | token/sec and quality numbers wait for prefill, decode, logits, and generation |
| a model artifact repository | real weights and generated GGUFs stay operator-local |

## Measurement Posture

YVEX does not publish token/sec or model-quality numbers yet because the
generation path does not exist. That is not a lack of ambition; it is the
minimum discipline needed for useful numbers. The measurements that are valid
today are correctness and regression checks at runtime boundaries the engine
actually owns.

| Boundary | Valid measurement | Invalid claim |
| --- | --- | --- |
| Fixture graph | exact output values and checksum | model quality |
| Selected embedding | raw-artifact reference checksum and sample values | inference throughput |
| Embedding plus RMSNorm | checksum and max-diff against independent reference | token/sec or generation speed |
| Token input | parsing, bounds, token selection, tokenizer fixture behavior | prefill or decode |

Later benchmarks need the full context: artifact identity, backend, qtype,
context length, machine, command, and reproducibility notes. Until prefill,
decode, logits, and generation exist in the same operator path, the honest
benchmark result is no published benchmark number.

## Provider Boundary

`./yvexd` is an early provider/status boundary. It is useful to wire process
health, metrics, model listing, direct paths, and registry aliases before
generation exists, but it must keep saying generation is unavailable. The server
surface is part of the engine shape; it is not the product pretending to be
done.

## Validation

Use the standard validation gate before committing changes:

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
make check-cuda
```

`make check-cuda` requires a CUDA-capable host. The artifact guardrails are
simple: no real model weights, no generated model GGUFs, no local registries,
and no generated reports/logs/build output should be tracked. Tracked GGUFs are
tiny fixtures under `tests/`.

## Source Layout

- `yvex_cli.c`: CLI entrypoint and command wiring.
- `yvexd.c`: provider/status daemon entrypoint.
- `yvex_*.c`: compact implementation modules.
- `cuda/`: CUDA host bridge and kernel unit.
- `gguf/`: GGUF parser, conversion, family mapping, and quant policy.
- `include/yvex/`: public C API.
- `tests/`: compact runners, fixtures, vectors, and CLI coverage.
- `docs/`: API, contract, operator runbook, and internal spine.

The layout is intentionally compact. As the runtime grows, source boundaries
should follow ownership boundaries: CLI code should not own backend logic,
server code should not duplicate runtime wiring, and tests should keep parser
fixtures, runtime fixtures, evaluation vectors, and benchmarks distinct.

## More Documentation

- `docs/operator-runbook.md`: command-first workflow for local artifacts,
  materialization, engine/session attachment, graph proofs, daemon status, and
  validation.
- `docs/api.md`: public C API, ownership rules, report structs, backend
  capabilities, token input, materialization summaries, and graph result
  surfaces.
- `docs/contract.md`: CLI, filesystem, registry, backend, server, and runtime
  behavior contract.
- `MODEL_ARTIFACTS.md`: external artifact cards, selected pressure artifacts,
  digest facts, tensor facts, and validation posture.
- `AGENTS.md`: operating rules for humans and coding agents working in this
  repository.
- `docs/spine.md`: internal delivery map; not public product documentation.

## License

YVEX is licensed under the MIT license.
