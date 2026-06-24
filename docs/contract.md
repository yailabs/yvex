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
