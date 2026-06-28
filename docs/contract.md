# YVEX Runtime Contract

## 1. Scope

YVEX is a CLI-first local runtime for open-weight model artifacts. It exposes a
C library, a command-line tool, and a server/provider daemon shell.

## 2. Command Contract

```text
make builds ./yvex and ./yvexd
./yvex is the CLI
./yvexd is the server/provider daemon
./yvex help and ./yvex commands are the command source of truth
```

Model-reference commands may accept registered model aliases where model
artifact selection is supported. `chat` uses the current selected registry model
when `--model` is omitted. `yvexd --model FILE_OR_ALIAS` resolves explicit
filesystem paths and registered aliases.

## 3. Filesystem Contract

```text
build/ is generated
.yvex/ is ignored local operator state
./yvex and ./yvexd are generated root binaries
model artifacts stay outside the repository
tests/fixtures may contain tiny fixtures only
```

Local registry files are machine-local and may contain absolute paths. They must
not be committed.

GGUF parser, artifact tooling, and model-family tensor mapping live under
`gguf/`. Tests use compact C/CLI runners, small fixtures, and stable vectors
under `tests/`.

## 4. Backend Contract

```text
CPU reference backend is the correctness baseline
CUDA implementation lives under cuda/
CUDA host bridge code stays in .c files
CUDA device kernels live in .cu files
CUDA availability is optional for baseline validation
backend failures return structured errors
materialization does not imply graph execution
```

CUDA support currently covers device probing, tensor allocation, tensor
movement, and a kernel-backed parity subset. It does not imply matmul,
attention, full model execution, generation, or benchmark support.

Engine/session diagnostics may attach selected materialized weights as
engine-owned runtime state. Attached weights prove backend residency under the
engine lifecycle; they do not imply graph execution, prefill, decode, logits,
sampling, generation, or inference readiness. Sessions may report the engine's
attachment summary, but they do not own or free engine-attached weights.

Fixture graph execution is available for deterministic controlled graph paths.
It proves planned embed-node ordering, backend dispatch, output allocation,
readback, and comparison-friendly output values. It does not imply real-model
graph execution, prefill, decode, logits, sampling, generation, or inference
readiness.

Real partial graph execution is implemented for the selected token-embedding
segment. It executes a constrained scheduled graph segment over engine-attached
`token_embd.weight`, converts the selected `F16` embedding row to an `F32` output
summary, and compares backend output against a raw-artifact reference slice. This
proves real selected tensor participation in scheduled graph work. It does not
imply prompt prefill, KV runtime, decode, logits, sampling, generation, server
generation, benchmark readiness, or full model execution.

### Artifact integrity baseline

YVEX validates baseline `GGUF` structural integrity before artifact paths that
read tensor payloads. The baseline validator checks file size, `GGUF`
parseability, tensor directory consistency, tensor name uniqueness, supported
rank/dims/dtype, checked byte-count math, tensor offset/range bounds, and
required tensor readiness for selected embedding partial graph execution.

This is not a supply-chain security guarantee. It does not prove model quality,
author identity, malware absence, cryptographic provenance, or safe execution of
untrusted code. It prevents runtime paths from trusting structurally inconsistent
artifacts.

YVEX maintains a tiny `GGUF` corruption fixture suite for repository
validation. The suite proves that parser, integrity, materialization, and graph
entry paths reject invalid artifacts before trusting tensor payloads. It is not
fuzzing and does not claim complete malicious-input coverage.

Tensor byte ranges are validated through a canonical calculation before runtime
paths trust tensor payloads. The calculation checks element count, dtype-size
multiplication, tensor byte count, `tensor_data_offset` plus tensor-relative
offset, end offset, file bounds, and alignment where applicable.
Materialization uses that validated range before allocation/copy. Real partial
graph execution also validates the selected token slice range before reading the
raw-artifact reference slice.

Tensor shape and dtype accounting is validated before byte-range validation and
runtime payload reads. YVEX rejects unsupported rank, zero dimensions,
element-count overflow, dtype-size ambiguity, tensor byte-count overflow, and
selected embedding shape/dtype incompatibility. Storage dtype recognition is
separate from runtime compute support: `F16 token_embd.weight` is supported for
the selected embedding segment, while full `F16` model execution and full
quantized inference remain unsupported.

### Artifact identity baseline

YVEX records local file identity for registered model aliases using file size
and SHA-256 digest. Registry identity is local operator evidence, not
supply-chain trust. A digest match proves that the current bytes match the bytes
recorded or expected by YVEX; it does not prove who created the artifact,
whether the artifact is safe, or whether the model is correct.

Safety-critical alias paths verify registered identity before reading tensor
payloads for materialization, engine/session attachment, gate checks, or graph
execution. If a registered alias lacks digest identity or the current file
digest no longer matches the registry, the command must fail before backend
allocation or graph execution.

Registered aliases also carry a local metadata summary. `models verify` compares
that recorded summary against current artifact facts and reports metadata drift
separately from digest drift: support level, architecture, tensor count, known
tensor bytes, primary tensor name/role/dtype/rank/dims/bytes, and selected
embedding readiness. Metadata drift diagnostics are local operator evidence, not
remote provenance. A metadata match does not prove model quality, full model
support, author identity, or supply-chain security.

Safety-critical alias paths that depend on the registered metadata summary must
fail if metadata drift invalidates the alias assumptions. Raw-path operations do
not require registry metadata; they rely on structural integrity and explicit
expected digest checks when provided.

### Materialization integrity gate

Materialization is gated by artifact integrity. Before backend allocation, YVEX
checks structural integrity, local identity when available, registry metadata
drift when using aliases, shape/dtype accounting, tensor byte ranges, selected
tensor readiness, and backend availability. If preflight fails, backend
allocation must not be attempted.

If allocation or transfer fails after preflight, materialization must attempt
cleanup and report the failure phase. The gate reports whether allocation,
transfer, and cleanup were attempted, plus planned, allocated, and transferred
byte counts where available. This is local backend lifecycle behavior; it is not
inference readiness, full model support, or supply-chain security.

### Graph execution integrity guard

Graph execution is guarded by artifact integrity. Fixture graph and real
selected embedding partial graph execution run graph-specific preflight before
dispatch: structural integrity, identity when using aliases, metadata drift,
shape/dtype accounting, tensor range validation, selected token slice
validation, backend availability, backend op support, output allocation size,
and reference read bounds. Preflight failure must prevent dispatch.

Post-preflight failures must attempt cleanup and report the graph phase. This
guard applies only to the implemented controlled fixture and selected embedding
partial graph paths. It is graph-entry safety, not full graph safety, inference
readiness, full model support, or supply-chain security.

### Integrity regression harness

The integrity module has a consolidated regression harness that exercises
structural corruption, digest drift, metadata drift, materialization gate
failures, graph guard failures, and cleanup/repeat behavior. The harness is
regression coverage for implemented boundaries; it is not fuzzing, sandboxing,
complete malicious-input coverage, or supply-chain security.

## 5. Server Contract

`./yvexd` exposes a provider/status shell:

```text
GET /health
GET /metrics
GET /v1/models
```

Generation endpoints remain unsupported.

## 6. Console Contract

```text
prompt is a diagnostic renderer
run is accepted-only diagnostics until inference exists
chat is a diagnostic console and future canonical REPL
plain text must not produce fake generated output
```

Future line editing belongs only to CLI/console code. It must not enter the
public C API, backend core, or server/headless path.

## 7. Model Registry Contract

```text
registry default: .yvex/models.local.json
override: YVEX_MODELS_REGISTRY
aliases are local convenience references
aliases resolve to external artifact paths
model artifacts stay external
```

Resolution order for explicit model references:

```text
existing filesystem path
registered alias
clear not-found error with list hint
```

No command may silently select an arbitrary model.
When `chat` is invoked without `--model`, it may use only the current selected
registry alias. If no model is selected, it must fail with a clear operator
hint.

## 8. Output Contract

```text
stdout: command payload and machine-readable output when requested
stderr: errors and diagnostics
logs/artifacts: build or runtime output only when requested
```

JSON/JSONL output must not mix logs or progress lines into stdout.

## 9. Validation Contract

Baseline:

```sh
make check
make smoke
git diff --check
```

CUDA-capable hosts:

```sh
make check-cuda
```

Documentation and artifact guardrails:

```sh
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected: no tracked large model artifacts; tracked GGUF files are tiny fixtures
only.

## 10. Claim Contract

Do not claim these without implementation and command proof:

```text
full model support
inference
generation
OpenAI-compatible generation
benchmark performance
execution_ready: true
```

Selected tensor materialization is not full model support. Backend residency is
not inference readiness.
