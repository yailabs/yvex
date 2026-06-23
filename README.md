# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.

The project is built around a simple idea: local inference should be inspectable from the
first byte of a model file to the last byte of a runtime result. A local runtime should
not merely accept a GGUF file and call it “supported”. It should be able to state where
the weights came from, how tensors were mapped, what was validated, what was
materialized, which backend owns memory, what is executable, and what still fails.

YVEX is not a model zoo, not a wrapper around another runtime, and not an API
compatibility layer. It is a correctness-first local execution substrate. The current
repository already implements artifact inspection, GGUF parsing, open-weight intake tools,
selected-tensor emission, CPU/CUDA materialization, model registry, runtime
diagnostics, and a provider shell. Full model execution, prefill, decode, sampling, and
generation are still ahead.

YVEX is intentionally strict about support claims. If behavior is not visible in source
code, tests, and command-visible proof, YVEX does not claim it.

## Motivations

Very capable open-weight models now exist, but local runtime stacks around them still need
clear boundaries. A file may parse and still not be executable. A model may materialize and
still not be runnable. A CUDA path may allocate memory while implementing only part of the
runtime. Benchmark output can look good while critical inference segments are missing.

YVEX exists to make those boundaries explicit.

First is control. A local runtime should own parsing, tensor inventory, model descriptor,
backend residency, and failure reporting. When something fails, the system should return a
specific reason: unsupported dtype, missing tensor, invalid range, CUDA unavailable,
materialization incomplete, graph path missing, decode not implemented. Silent fallback and
vague claims are unacceptable.

Second is reproducibility. Open-weight work should start from official source, not random
artifacts. YVEX treats the original model source, the native weight inventory, the conversion
recipe, the GGUF artifact, and materialization proof as a linked chain. Every step should be
inspectable.

Third is local-machine realism. Running large models locally is not just a model-format
problem. It is memory, bandwidth, cache, quantization, backend, and machine-boundary
problem. YVEX is designed to grow into that space: CUDA first, CPU for reference, future KV
work, future large-context execution, and eventual real measurements rather than marketing
promises.

Fourth is engineering honesty. It is acceptable for YVEX to report “unsupported”. It is not
acceptable to imply selected-tensor materialization is inference, or that a server shell is a
full generation server, or that CUDA memory movement is a full CUDA model backend.

## Status

YVEX is pre-inference runtime infrastructure.

Implemented behavior is enough to inspect and reason about model artifacts, parse GGUF metadata
and tensor directories, classify tensors, build model descriptors, run a fixture tokenizer
path, build graph and memory-plan substrates, open CPU and CUDA backends, materialize selected
weights, and validate selected GGUF artifacts through command-visible gates.

The implemented CUDA path is real but narrow. CUDA code lives under `cuda/`, and there is a real
CUDA kernel translation unit. The current proof covers CUDA device probing, tensor movement, an
F32 embedding kernel path, and CPU/CUDA parity for that path. This is not full CUDA model
execution.

The implemented provider daemon is also narrow. `./yvexd` exposes status-oriented behavior and
a provider-shell structure. Generation endpoints remain unsupported until inference exists.

Current status in one sentence:

```text
YVEX can inspect, validate, emit selected artifacts, materialize selected weights, and
prove backend residency; it cannot yet run a full model.
```

Implemented now:

```text
artifact loading
gguf header, metadata, and tensor-directory parsing
dtype, tensor, and model descriptor inspection
tokenizer fixture path and prompt rendering
graph and memory-plan substrate
CPU reference backend
CUDA backend with tensor movement and a kernel unit
selected-tensor GGUF emission
selected-tensor CPU/CUDA materialization
open-weight source manifest
native safetensors inventory
GGUF template validation
tensor mapping and family mapping
quantization policy and imatrix manifests
model registry and one-shot alias resolution
model gate and materialization gate diagnostics
status-oriented provider daemon
compact test runners and stable tiny vectors
```

Not implemented yet:

```text
full-model execution
prefill
decode
sampling
generation
vendor-compatible generation APIs
token-latency and throughput inference benchmarks
KV-cache execution
paged KV
host spill
large-model streaming
distributed inference
```

`execution_ready` must remain false until a real executable graph path exists.

## Build and baseline usage

```sh
make
./yvex version
./yvex commands
```

`./yvex` is the operator/developer CLI.

`./yvexd` is the local server/provider daemon.

```sh
./yvexd --help
```

CUDA validation requires a CUDA-capable host:

```sh
make check-cuda
```

Baseline validation is:

```sh
make check
make smoke
```

## Model weights and open-weight intake

YVEX does not treat GGUF as the only source of truth. A model path starts from an external
source and records how it becomes a local runtime artifact.

The intake chain is:

```text
official/open weights
 -> source manifest
 -> native weight inventory
 -> architecture/family tensor mapping
 -> quantization policy
 -> optional calibration / imatrix metadata
 -> GGUF emission or validation
 -> materialization
 -> execution
```

The repository contains tools for this chain, but only selected parts are complete today.

Create a source manifest:

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --license LICENSE \
  --model-card https://... \
  --local-path /path/to/native/source \
  --node local \
  --status in-progress \
  --out /path/to/manifest.json
```

Inspect native weights:

```sh
./yvex native-weights --source /path/to/native/source
./yvex native-weights --source /path/to/native/source --tensor embed.weight
```

Inspect a GGUF template:

```sh
./yvex gguf-template inspect --template /path/to/template.gguf
./yvex gguf-template validate --template /path/to/template.gguf
```

Map native tensors:

```sh
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source --limit 20
./yvex tensor-map \
  --arch deepseek4 \
  --native-source /path/to/native/source \
  --template /path/to/template.gguf
```

Plan or emit a selected conversion:

```sh
./yvex convert plan \
  --arch deepseek4 \
  --native-source /path/to/native/source \
  --out-plan /tmp/yvex-plan.json

./yvex convert emit \
  --arch deepseek4 \
  --native-source /path/to/native/source \
  --tensor model.embed_tokens.weight \
  --target-qtype F16 \
  --out /tmp/yvex-selection.gguf \
  --overwrite
```

YVEX does not yet claim full-model conversion or full-model support. Current real-model support is
selected-tensor materialization and validation.

Large model files do not belong in this repository. External artifact cards are kept in
`MODEL_ARTIFACTS.md`.

## Local model registry

Model paths are long and machine-local. YVEX uses a local registry so operators can use aliases
instead of long paths.

Register a local artifact:

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex models use local-model
```

Use the alias in one-shot commands:

```sh
./yvex inspect local-model
./yvex metadata local-model
./yvex tensors local-model
./yvex materialize --model local-model --backend cuda
```

An alias is a local reference, not a support claim.

## Backends and machine targets

The CPU backend is the reference and diagnostic path. It is useful for correctness, simple
materialization, parser validation, and comparing backend behavior.

The CUDA backend is the primary accelerated direction. It currently supports device probing,
memory stats, tensor allocation, tensor movement, and a scoped F32 embedding-kernel parity path.
It is not yet a full model backend.

Future runtime priorities are the real bottlenecks:

```text
embedding
RMSNorm
RoPE
attention
quantized matmul
MoE routing and experts
logits projection
KV-cache movement
prefill
decode
```

Machine posture:

```text
CPU-only Linux:
  reference and diagnostics

CUDA host/GPU:
  primary accelerated target

Metal-like and other accelerators:
  future possible backends, not implemented
```

Validate these paths with:

```sh
make
make check
make smoke
make check-cuda
```

## Quantization and artifact policy

YVEX treats quantization as an explicit policy, not an accidental artifact property.

The project already has manifest and validation surfaces for quantization policy, qtype
support, imatrix metadata, and external quantization job records. That does **not** mean a full
native quantization execution suite is complete.

The operational distinction is:

```text
storage support:
  can parse or account for a qtype

materialization support:
  can copy tensor bytes into backend memory

compute support:
  can execute kernels using that qtype

model support:
  can execute the full model path correctly
```

Those levels are intentionally separated. YVEX should not collapse them into a single generic
"supported" label.

## Performance direction

YVEX does not publish inference speed numbers yet because inference is not implemented.

Current measurable behavior is lower-level:

```text
GGUF parse and inspection time
tensor directory scan
automated materialization time
materialized byte count
backend-allocated bytes
CUDA memory before/after
CPU/CUDA parity for implemented kernels
trace/profile JSON for diagnostic runtime paths
```

A future speed profile should only appear when the corresponding runtime path exists:

```text
Machine              Backend   Artifact / qtype       Context       Prefill       Generation
CUDA target (GPU)    CUDA      selected future GGUF    TBD           TBD           TBD
CPU reference        CPU       tiny fixture            diagnostic    N/A           N/A
```

## Memory and large-model behavior

Large local models are constrained by memory before they are constrained by style.

A runtime must account for:

```text
model weights
non-routed weights
routed experts
KV cache
graph scratch buffers
activations
backend allocator overhead
host/device transfer
host spill or disk staging if implemented
```

YVEX does not yet implement SSD streaming, paged KV, disk KV, or host spill execution. These
are central design pressures already represented in the implementation direction.

The intended direction is to make memory state explicit:

```text
what is resident
what is materialized
what is skipped
what is unsupported
what would be required for a larger context
what fails because memory is insufficient
```

Rather than binary fit/no-fit logic, the future target is measurable state:

```text
which tensors are resident, which cache is hot, which cache is cold,
how much context is affordable, and what speed tradeoff follows
```

## KV cache and long context

KV cache is a first-class runtime concern for YVEX, but execution KV is not implemented yet.

The planned path should separate:

```text
static KV size estimation
CUDA KV allocation
paged/block KV
host spill
disk cold-cache behavior
KV quantization policy
session checkpoint shape
```

KV must be inspectable, sized, validated, and measured when added. Until then, YVEX should
state unsupported state rather than imply capability.

## Test vectors and correctness

YVEX tests are driven by compact runners and tiny deterministic vectors:

```text
tests/test.c        baseline non-CUDA C runner
tests/test_cuda.c   CUDA runner
tests/cli.sh        CLI smoke runner
tests/vectors/      stable tiny vectors
tests/fixtures/     tiny structural fixtures
```

Run:

```sh
make check
make smoke
make check-cuda
```

Vectors are used for semantic regression control before runtime ambiguity grows. Today they cover
small parser, tokenizer, GGUF, and CUDA cases.

Large model artifacts do not belong in `tests/vectors/`.

## Debugging and diagnosis

Start with inspection:

```sh
./yvex inspect local-model
./yvex metadata local-model
./yvex tensors local-model
```

Check tokenizer and prompt behavior:

```sh
./yvex tokenizer local-model
./yvex tokenize local-model --text "hello world"
./yvex prompt local-model --system "You are helpful" --user "hello"
```

Check planning and backend state:

```sh
./yvex graph local-model
./yvex plan local-model --backend cuda
./yvex cuda-info
```

Check materialization:

```sh
./yvex materialize --model local-model --backend cpu
./yvex materialize --model local-model --backend cuda
```

Use gates when validating artifacts:

```sh
./yvex model-gate check --model-name test --artifact /path/to/model.gguf --out /tmp/model-gate.txt
./yvex materialize-gate check --model-name test --artifact /path/to/model.gguf --out /tmp/materialize-gate.txt
```

The first questions are always:
- did the artifact parse,
- did tensors map,
- did materialization happen,
- did the backend report precise state?

## CLI surface

The CLI is the command-visible proof surface.

```sh
./yvex commands
./yvex help
./yvex help inspect
./yvex help models
./yvex help materialize
```

Representative command groups:

```text
artifact inspection:
  inspect, metadata, tensors

tokenizer and prompt:
  tokenizer, tokenize, detokenize, prompt

planning:
  graph, plan

backends:
  backend, cuda-info

diagnostic runtime:
  engine, session, run, chat

open-weight and GGUF tooling:
  source-manifest, native-weights, gguf-template, gguf-emit,
  tensor-map, convert, qtype-support, quant-job, quant-policy, imatrix

local registry:
  models

validation gates:
  model-gate, materialize-gate
```

Commands may report unsupported behavior. That is part of the contract.

## Server/provider daemon

`./yvexd` is the local provider daemon shell.

It currently exposes status-oriented behavior. Generation endpoints remain unsupported until
inference exists.

```sh
./yvexd --help
./yvexd --version
```

The server is intended as a local provider boundary for higher-level systems, but it must not
pretend compatibility before runtime can generate valid outputs.

## Source layout

```text
yvex_cli.c             CLI entrypoint
yvexd.c                daemon entrypoint
yvex_*.c               compact C implementation units

cuda/                  CUDA host bridge, tensor storage, and kernels
gguf/                  GGUF parser, conversion, quant policy, naming, and family mapping

include/yvex/          public C headers
tests/                 compact runners, fixtures, and vectors
docs/                  API, runtime contract, and internal spine
```

Generated and local-only state is ignored:

```text
build/
.yvex/
./yvex
./yvexd
```

## Documentation

```text
README.md             project overview and usage
MODEL_ARTIFACTS.md    external artifact cards and support posture
AGENTS.md             operating rules for humans and coding agents
docs/api.md           public C API surface
docs/contract.md      runtime/backend/filesystem/CLI contract
docs/spine.md         internal delivery roadmap
```

## License

YVEX is licensed under the MIT license.
