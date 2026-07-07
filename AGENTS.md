# AGENTS.md

## 0. Repository Contract

YVEX is a native C local inference engine for open-weight model artifacts.

The repository must become more executable, more tested, or more internally
coherent after each patch.

Documentation records implemented truth. Documentation does not replace
implementation.

A delivery that only changes language is valid only when the language is an
execution contract needed before code can safely change.

## 1. Execution Order

Default order:

1. code
2. tests
3. docs

Docs-only work is allowed only for scope, release doctrine, claim boundaries,
operator contracts, or explicit user-requested documentation.

## 2. Ownership

Find the owner file before editing.

Root contains project metadata, public docs, Makefile, tests, include, and
binaries only.

C implementation lives under src/.

Public headers live under include/yvex/.

Private headers live beside the source module that owns them.

No new root C file.

No new root private header.

No compatibility shell at root.

Root yvex_*.c files are forbidden after TOPOLOGY.FS.0.

Root yvex_*_private.h files are forbidden after TOPOLOGY.FS.0.

Forbidden file patterns:

```text
*_commands.c
yvex_cli_*.c outside approved src/cli/io writer files
yvex_output.c
yvex_render.c
source_*_new.c
src/backend/cuda/*_new.cu
```

Owner map:

```text
src/cli/yvex_cli.c
  top-level lookup, short help, argv dispatch only

src/cli/commands/*.c
  CLI command adapter slots only; no domain structs, domain APIs, backend
  opens, runtime state, or artifact/source/model behavior

src/cli/render/*.c
  normal/table/audit/trace renderers for CLI-owned output shapes

src/cli/io/yvex_cli_out.c
src/cli/io/yvex_cli_error.c
src/cli/io/yvex_cli_json.c
src/cli/io/yvex_cli_table.c
src/cli/io/yvex_cli_log.c
  approved direct operator output writer files

src/cli/catalog/*.def
  option, example, boundary, status, and static command metadata lists only

src/cli/schema/*.json
  CLI plumbing schemas only, without uniform JSON capability claims

src/cli/commands/yvex_model_target_cli.c
  model-target CLI grammar, usage/help, output rendering, and report commands

src/core/yvex_operator_private.h
  private shared operator helper declarations and domain-owned command
  entrypoint declarations

src/core/yvex_operator_render_private.h
  private shared operator text rendering helpers for existing domain reports

src/daemon/yvexd.c
  daemon entrypoint

src/server/yvex_server.c
  daemon/server behavior and provider boundary

src/accounts/yvex_accounts.c
  local provider account boundary

src/runtime/yvex_runtime.c
  runtime coordination

src/graph/yvex_graph.c
  graph construction, execution proofs, op probes

src/backend/yvex_backend.c
  backend abstraction and backend reports

src/backend/cuda/
  CUDA backend, kernels, tensor movement, CUDA op implementations

src/backend/cuda/cuda_kernels.cu
  CUDA device kernels

src/backend/cuda/cuda_ops.c
  CUDA host launches and validation

src/model/yvex_model.c
  dtype registry, model descriptors, tensor role names/classification, tensor
  table, materialized weights

src/model/yvex_model_artifacts.c
  artifact status, artifact gates, selected/full artifact reports

src/artifact/yvex_artifact.c
  artifact IO, inspect, metadata, tensor command surfaces

src/artifact/yvex_artifact_identity.c
  artifact identity and digest behavior

src/artifact/yvex_artifact_integrity.c
  artifact integrity, corruption/refusal reports

src/gguf/
  GGUF parsing, conversion, quant/intake internals

src/gguf/tools.c
  GGUF tooling command surfaces

src/gguf/conversion.c
  selected tensor conversion bridge and mapping helpers

src/gguf/quant.c
  quantization manifest and policy internals

src/source/yvex_source.c
  source manifests, source pressure, source evidence, native header inventory

src/tokenizer/yvex_tokenizer.c
  tokenizer metadata and tokenizer command surfaces

src/tokenizer/yvex_token_input.c
  token/prompt input validation

src/generation/yvex_prefill.c
  prefill state and prefill reports

src/generation/yvex_kv.c
  KV shape, ownership, append/read, lifecycle, capacity diagnostics

src/generation/yvex_decode.c
  decode step boundary over existing KV-backed transformer state

src/generation/yvex_logits.c
  logits buffer ownership and diagnostics

src/generation/yvex_sampling.c
  sampling over logits

src/generation/yvex_generation.c
  generation loop integration

src/eval/yvex_eval.c
  eval harness after runtime generation exists

src/bench/yvex_bench.c
  benchmark harness after measured runtime paths exist

src/metrics/yvex_metrics.c
  metrics and counters

src/metrics/yvex_profile.c
  profile output

src/runtime/yvex_chat.c
  diagnostic console and future runtime-backed REPL shell
```

## 3. Source File Contracts

Every source file must declare Owner, Owns, Does not own, Invariants, Boundary.

The file header is an executable ownership guard, not decoration.

A file may not grow behavior that violates its header.

If a row needs behavior outside the current file contract, update topology first
or move the behavior to the owner module.

Every non-trivial function must have a function contract before implementation.

Function comments describe the contract, not the obvious C syntax.

Comments must name allocation, mutation, IO, printing, side effects, failure,
cleanup, and capability boundary where relevant.

Do not write marketing prose in source comments.

Do not write vague comments such as:

```text
handles things
does stuff
helper
process data
run logic
```

## 4. C Implementation Rules

Keep ownership explicit.

Keep allocation and cleanup in the owner module.

Check allocation failure.

Use existing error style.

Use existing naming style.

Use existing parser/exit-code style.

Do not add global state unless the owner module already owns that state.

Do not introduce a second state field for the same fact.

Do not hide byte-range arithmetic behind vague helpers.

Do not read tensor payload bytes in header-only rows.

Do not treat lexical tensor names as role mapping unless the row owns mapping.

Use precise failure classes:

```text
missing-source-path
unsupported-output-mode
malformed-safetensors-header
source-payload-not-loaded
unsupported-family
invalid-release
unsupported-runtime-stage
unsupported-generation-family
```

Do not use vague failures:

```text
failed
bad input
not ready
error
```

## 5. CLI Rules

yvex_cli.c owns dispatch only.

CLI command grammar lives under src/cli/.

Manual operator output belongs to src/cli.

Domain modules must not call:

```text
printf
fprintf
vprintf
vfprintf
puts
fputs
putchar
perror
```

Domain modules do not own usage text, porcelain, plumbing, audit walls, or
argv parsing.

Domain modules may build facts and return report structs.

CLI command adapters parse argv and call domain APIs.

CLI renderers serialize facts into normal/table/audit/plumbing output.

CLI catalog files own lists of options, examples, boundaries, statuses, and
static command metadata.

No domain file may include src/cli/catalog files.

No new direct output call may be added outside approved writer files:

```text
src/cli/io/yvex_cli_out.c
src/cli/io/yvex_cli_error.c
src/cli/io/yvex_cli_json.c
src/cli/io/yvex_cli_table.c
src/cli/io/yvex_cli_log.c
src/core/yvex_file_writer.c for non-operator file serialization only
```

Normal output is compact.

Audit output carries evidence.

Table output is for aligned list-like rows.

No audit wall in normal output.

No JSON unless the row owns and tests JSON.

No color unless the row owns and tests color.

No metrics output unless the row owns and tests metrics.

No command may imply lower runtime capability.

Output modes:

```text
normal
table
audit
```

CLI test classes:

```text
normal output
audit output
parser failure
exit code
unsupported flag
unsupported output mode
```

## 6. Runtime And Backend Rules

Primitive proof is not runtime support.

Fixture proof is not runtime support.

Selected-slice proof is not full-runtime support.

Diagnostic runtime is not full runtime.

CUDA primitive hardening is not CUDA generation.

Backend allocation is not graph execution.

Graph execution is not generation.

Generation requires prefill, KV, decode, logits, sampling, append, stop,
cleanup.

Preserve existing CUDA Driver API style.

A CUDA row must include host binding, reference comparison, failure path,
cleanup, and claim guard.

Metal and ROCm are future/backend lanes until explicit rows promote them.

No Metal or ROCm support claim without implementation and tests.

## 7. Source, Tensor, And Artifact Rules

Header-only means header-only.

Source intake is not source verification.

Source manifest is not payload trust.

Tensor metadata inventory is not tensor role mapping.

Tensor role mapping is not artifact emission.

Qtype policy is not quantization.

Generation-capable artifact is not runtime generation.

External GGUF is not YVEX support evidence.

External runner output is not YVEX runtime proof.

Never commit:

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
local registry files
downloaded model files
generated PTX/build outputs
```

Allowed:

```text
tiny GGUF fixtures under tests only
```

## 8. Evidence Stages

Canonical stages:

```text
unsupported
planned
blocked
report-only
header-only
source-intake
fixture-proof
selected-slice-proof
diagnostic-runtime
generation-capable-artifact-ready
full-runtime-candidate
runtime-generation-ready
eval-ready
benchmark-ready
release-ready
not-measured
```

Use the lowest true stage.

Do not promote stage by wording.

Do not invent synonyms.

Do not write "ready" without a scoped prefix.

## 9. Claims

A claim requires implementation, tests, command proof, and documented boundary.

Forbidden unless true:

```text
full model support
inference ready
model ready
generation ready
prefill ready
decode ready
CUDA runtime ready
CUDA generation
Metal runtime ready
Metal support
Qwen supported
Gemma supported
DeepSeek supported
DeepSeek generation implemented
multi-family generation implemented
benchmark measured
throughput achieved
release_ready: true
execution_ready: true
generation_ready: true
```

Use lower-scope language:

```text
report-only
header-only
source-intake
diagnostic-runtime
selected-slice-proof
fixture-proof
bounded primitive
source-pressure
target-profile
unsupported
not-measured
```

## 10. Docs

Docs record current truth.

Docs do not create capability.

Docs changes must be minimal unless the row owns a doctrine/contract boundary.

Do not duplicate a canonical document.

Do not expand roadmap prose.

Do not write marketing copy.

Do not write AI-style filler.

Canonical docs:

```text
docs/spine.md
docs/v010-release-doctrine.md
docs/model-families.md
docs/api.md
docs/contract.md
docs/operator-runbook.md
docs/cli-output-architecture.md
MODEL_ARTIFACTS.md
AGENTS.md
```

Docs entry shape:

```text
what is true
how to prove it
what boundary remains
next blocker
```

## 11. Tests

Tests are part of implementation.

A behavior without tests is not complete.

A refusal path without tests is not complete.

A command without output tests is not complete.

A docs claim without a guard is not stable.

Runtime/backend tests:

```text
reference result
implementation result
tolerance if numeric
edge case
failure/refusal path
cleanup where relevant
```

Source/model tests:

```text
missing source
fake source
malformed input where relevant
normal/table/audit preservation
no payload loading where relevant
no readiness claim
```

CLI tests:

```text
normal output
audit output
parser failure
exit code
compactness where output surface changes
```

## 12. Validation

Baseline:

```sh
git diff --check
make
make smoke
make check
make check-docs
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
```

CUDA-capable hosts:

```sh
make check-cuda
```

Artifact guardrail:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected artifact guardrail:

```text
no tracked large model artifacts
tracked GGUF files are tiny test fixtures only
```

## 13. Review Failure

A patch fails review if it:

```text
edits the wrong owner module
adds behavior to yvex_cli.c that belongs elsewhere
creates new source files without ownership need
adds docs without implementation or explicit doctrine boundary
adds capability claims without executable proof
commits model artifacts or local state
adds direct operator output outside approved CLI writer files
adds usage/help text, argv parsing, or CLI catalogs outside src/cli
dumps audit fields into normal output
duplicates state fields instead of consolidating
introduces abstraction without removing complexity
breaks normal/table/audit behavior
reads tensor payloads in header-only/source-report rows
claims runtime/generation/benchmark/release readiness without proof
```

## 14. Final Rule

Make YVEX more real.

Less roadmap.
Less placeholder.
Less duplicate state.
Less prose.

More executable code.
More owner-file coherence.
More reference tests.
More bounded primitives.
More honest boundaries.
