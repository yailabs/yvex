# AGENTS.md

## Project Rule

YVEX is a C local runtime for open-weight model artifacts. Read
`docs/spine.md` before choosing implementation work. That file is the only
internal delivery map.

## Boundaries

YVEX owns local runtime work:

```text
artifact loading
GGUF parsing
tensor inspection
selected tensor materialization
backend diagnostics
server/provider status shell
```

YAI controls governance, authority, case state, operational memory, and tool
policy outside this repository.

## Hard No-Claims

Do not claim these without implementation, tests, command proof, and documented
limits:

```text
full model support
inference
generation
prefill
decode
benchmark performance
execution_ready: true
```

Current real-model support is selected-tensor materialization only.

## Artifacts

Do not commit model artifacts or local operator state:

```text
*.safetensors
*.bin
*.dat
external generated *.gguf
.yvex/
build/
server logs
pid files
external reports
```

External model artifacts are documented in `MODEL_ARTIFACTS.md`.
Do not publish personal absolute artifact paths in docs. Public docs may
describe local artifacts as operator-local and external to the repository, but
must not expose developer workstation paths.

## Commands

Build first:

```sh
make
```

Use repository-local binaries:

```sh
./yvex
./yvexd
```

Useful operator commands:

```sh
./yvex commands
./yvex models list
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvexd --help
```

## Validation

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

Guardrails:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected: no tracked large model artifacts; tracked GGUF files are tiny test
fixtures only.

## Work Rules

- Keep generated artifacts outside git.
- Keep public docs free of internal delivery IDs.
- Keep `execution_ready: false` until real execution exists.
- After every implementation step that changes operator-visible commands,
  workflows, diagnostics, validation, or runtime boundaries, update
  `docs/operator-runbook.md` in the same wave.
- Prefer precise failure classes over vague errors.
- Do not add docs sprawl; keep canonical docs in `docs/api.md`,
  `docs/contract.md`, `docs/operator-runbook.md`, and `docs/spine.md`.
