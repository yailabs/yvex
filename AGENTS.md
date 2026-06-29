# AGENTS.md

## Project Rule

YVEX is a native C local inference engine for open-weight model artifacts. Read
`docs/spine.md` before choosing implementation work. That file is the only
internal delivery map.

## Boundaries

YVEX owns local runtime work:

```text
artifact loading
GGUF parsing
tensor inspection
selected tensor materialization
engine/session weight attachment
bounded fixture and selected real-tensor graph execution
token input diagnostics
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

Current real-model support is bounded selected embedding and selected
embedding-plus-RMSNorm graph execution only. Full inference/generation remains
outside the current boundary.

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

## Source Ownership

Do not create a new root source file for a delivery unless `docs/spine.md`
explicitly adds a new ownership domain. Prefer the canonical owner listed here:

```text
yvex_cli.c: top-level CLI grammar, help, dispatch, and small shared adapters
yvexd.c: daemon entrypoint only
yvex_server.c: daemon/server behavior and provider boundary
yvex_runtime.c: runtime coordination not owned by graph/model/backend/session-specific modules
yvex_graph.c: graph construction, graph execution proofs, op probes, references, graph reports
yvex_backend.c: backend abstraction and CPU backend ownership
cuda/: CUDA backend, kernels, tensor movement, CUDA op implementations
yvex_model.c: model descriptors, model-family runtime facts, registry-facing model behavior
yvex_model_artifacts.c: model artifact cards, selected/full artifact status, model gates
yvex_artifact*.c: artifact IO, identity, integrity, range/shape/dtype validation
gguf/: GGUF parsing, conversion, quant/intake tooling internals
yvex_source.c: source manifests and open-weight provenance
yvex_tokenizer.c: tokenizer metadata and fixture encode/decode
yvex_token_input.c: explicit token/prompt input validation
yvex_prefill.c: prefill state creation and prefill reports
yvex_kv.c: KV shape, ownership, append/read, lifecycle, capacity diagnostics
yvex_decode.c: decode step over existing KV-backed transformer state
yvex_logits.c: logits buffer ownership and diagnostics
yvex_sampling.c: deterministic and stochastic sampling over logits
yvex_generation.c: generation loop integrating decode, logits, and sampling
yvex_eval.c: eval harness after runtime generation exists
yvex_bench.c: reproducible runtime benchmark harness after measured paths exist
yvex_metrics.c: metrics and trace/profile counters
yvex_profile.c: profile output and runtime profile documents
yvex_chat.c: interactive accepted-only or future runtime-backed REPL shell
```

Domain `*_commands.c` files are argv/output adapters for `./yvex` only. They
must not become hidden owners of runtime state, graph behavior, artifact
validation, registry semantics, eval logic, or benchmark logic.

No `yvex_cli_*.c` or `yvex_cli_*.h` files. No command/proof logic may be hidden
in a file named runtime just because it is convenient. No eval or benchmark
claim may be introduced by creating skeleton files.

## Work Rules

- Keep generated artifacts outside git.
- Keep public docs free of internal delivery IDs.
- Keep `execution_ready: false` until full execution readiness is actually
  implemented, tested, command-proven, and documented.
- After every implementation step that changes operator-visible commands,
  workflows, diagnostics, validation, or runtime boundaries, update
  `docs/operator-runbook.md` in the same wave.
- Prefer precise failure classes over vague errors.
- Do not add docs sprawl; keep canonical docs in `docs/api.md`,
  `docs/contract.md`, `docs/operator-runbook.md`, and `docs/spine.md`.
