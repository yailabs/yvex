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
execution-ready true-state
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

Do not create new root source files during a delivery unless the spine first
adds a new ownership domain.

Do not create `*_commands.c` files.

Do not create `yvex_cli_*` files.

Do not create a private command catalog header.

`yvex_cli.c` owns only top-level CLI command lookup, short command listing,
top-level help, and argv dispatch. It does not own domain help text, runtime
helpers, model/artifact context helpers, graph probes, proof references,
materialization reports, registry behavior, conversion behavior, eval behavior,
or benchmark behavior.

Detailed command help and command behavior live in the domain module that owns
the underlying capability.

```text
yvex_cli.c: top-level CLI lookup, short help, command dispatch
yvexd.c: daemon entrypoint
yvex_server.c: daemon/server behavior and provider boundary
yvex_runtime.c: runtime coordination and runtime operator commands not owned by more specific modules
yvex_graph.c: graph construction, graph execution proofs, op probes, references, graph reports, graph help
yvex_backend.c: backend abstraction, CPU backend command, backend reports
cuda/: CUDA backend, CUDA info command, kernels, tensor movement, CUDA op implementations
yvex_model.c: model descriptors, model registry behavior, model command/help
yvex_model_artifacts.c: model artifact status, model gates, selected/full artifact reports
yvex_artifact.c: artifact IO, inspect/metadata/tensor command surfaces
yvex_artifact_identity.c: artifact identity and digest behavior
yvex_artifact_integrity.c: artifact integrity checks, integrity command/help, corruption/refusal reports
gguf/: GGUF parsing, GGUF tooling, conversion, quant/intake internals
yvex_source.c: source manifests and open-weight provenance
yvex_tokenizer.c: tokenizer metadata, tokenize/detokenize/prompt command surfaces
yvex_token_input.c: explicit token/prompt input validation and input command
yvex_prefill.c: prefill state creation, prefill command/help, prefill reports
yvex_kv.c: KV shape, ownership, append/read, lifecycle, capacity diagnostics, KV command/help
yvex_decode.c: decode step boundary over existing KV-backed transformer state
yvex_logits.c: logits buffer ownership and diagnostics
yvex_sampling.c: deterministic and stochastic sampling over logits
yvex_generation.c: generation loop integrating decode, logits, and sampling
yvex_eval.c: eval harness after runtime generation exists
yvex_bench.c: reproducible runtime benchmark harness after measured paths exist
yvex_metrics.c: metrics and trace/profile counters
yvex_profile.c: profile output and runtime profile documents
yvex_chat.c: interactive diagnostic console and future runtime-backed REPL shell
```

A delivery that adds command behavior to `yvex_cli.c` fails review. A delivery
that adds long command usage strings to `yvex_cli.c` fails review. A delivery
that adds a new `*_commands.c` file fails review.

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
