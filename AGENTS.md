# AGENTS.md

## Operating Rule

YVEX is a native C local inference engine for open-weight model artifacts.

Every delivery must make the repository more executable, more tested, or more
internally coherent. Documentation records implemented truth; it must not
substitute for implementation.

Default priority:

```text
code first
tests second
minimal docs after
```

Do not produce roadmap-only, doctrine-only, or report-only work unless the
current implementation genuinely requires a boundary contract before code can
safely change.

`docs/spine.md` is the internal delivery ledger. It is not permission to write
doc-heavy patches. Use it to understand the current state and to record
completed implementation state.

## Implementation Standard

A delivery must reduce a concrete implementation debt.

Good delivery targets:

```text
incomplete kernel
missing parser behavior
missing source inventory
missing tensor metadata
missing runtime primitive
missing host/device binding
missing validation path
duplicated output logic
unclear error class
uncovered edge case
```

Bad delivery targets:

```text
new terminology without behavior
roadmap expansion
status-field proliferation
duplicate report fields
new abstraction for imagined future use
large docs rewrite without executable change
```

Prefer bounded real implementation over placeholder behavior.

A small, safe, tested implementation is better than a large planned abstraction.

## Existing Owner Files First

Before creating a new file, find the existing owner module.

Do not create new root source files unless there is a real new ownership domain
and the user explicitly accepts it.

Do not split large root files merely because they are large.

Do not create:

```text
*_commands.c
yvex_cli_*.c
yvex_output.c
yvex_render.c
source_*_new.c
cuda/*_new.cu
src/
```

A new abstraction is allowed only when it removes existing complexity,
duplication, or unsafe coupling. It is not allowed merely to prepare for a
possible future.

## Internal Coherence

Patches must look native to the repository.

Preserve:

```text
existing C style
existing naming style
existing error handling
existing command grammar
existing output mode grammar
existing normal/table/audit split
existing Driver API CUDA style
existing owner-module boundaries
```

Do not introduce a second way to express the same state.

Avoid semantic duplication such as:

```text
generation: unsupported
generation_ready: false
full_model_generation: no
runtime_generation_status: unsupported
```

Use the canonical field already used by the owner module unless the delivery
explicitly removes or consolidates the older form.

## CLI Ownership

`yvex_cli.c` owns only:

```text
top-level CLI lookup
short command listing
top-level help
argv dispatch
```

`yvex_cli.c` must not own domain behavior, long usage strings, runtime helpers,
graph probes, model/artifact logic, materialization reports, source reports,
eval, or benchmark behavior.

A delivery fails review if it adds substantial command behavior to
`yvex_cli.c`.

Detailed command help and command behavior must live in the module that owns the
capability.

## Source Ownership Map

Use the existing owner module before creating anything new.

```text
yvex_cli.c
  top-level CLI lookup, short help, dispatch only

yvexd.c
  daemon entrypoint

yvex_server.c
  daemon/server behavior and provider boundary

yvex_accounts.c
  local provider account boundary, provider CLI discovery, login/status/ensure, non-secret account observations

yvex_runtime.c
  runtime coordination and runtime operator commands not owned by more specific modules

yvex_graph.c
  graph construction, graph execution proofs, op probes, graph reports, graph help

yvex_backend.c
  backend abstraction, CPU backend command, backend reports

cuda/
  CUDA backend, CUDA info command, kernels, tensor movement, CUDA op implementations

cuda/cuda_kernels.cu
  CUDA device kernels

cuda/cuda_ops.c
  CUDA host-side primitive launches and validation

yvex_model.c
  model descriptors, model target registry, model command/help, target reports

yvex_model_artifacts.c
  model artifact status, model gates, selected/full artifact reports

yvex_artifact.c
  artifact IO, inspect/metadata/tensor command surfaces

yvex_artifact_identity.c
  artifact identity and digest behavior

yvex_artifact_integrity.c
  artifact integrity checks, integrity command/help, corruption/refusal reports

gguf/
  GGUF parsing, GGUF tooling, conversion, quant/intake internals

yvex_source.c
  source manifests, source pressure reports, open-weight source evidence, provenance, footprint, native header inventory

yvex_tokenizer.c
  tokenizer metadata, tokenize/detokenize/prompt command surfaces

yvex_token_input.c
  explicit token/prompt input validation and input command

yvex_prefill.c
  prefill state creation, prefill command/help, prefill reports

yvex_kv.c
  KV shape, ownership, append/read, lifecycle, capacity diagnostics, KV command/help

yvex_decode.c
  decode step boundary over existing KV-backed transformer state

yvex_logits.c
  logits buffer ownership and diagnostics

yvex_sampling.c
  deterministic and stochastic sampling over logits

yvex_generation.c
  generation loop integrating decode, logits, and sampling

yvex_eval.c
  eval harness after runtime generation exists

yvex_bench.c
  reproducible runtime benchmark harness after measured paths exists

yvex_metrics.c
  metrics and trace/profile counters

yvex_profile.c
  profile output and runtime profile documents

yvex_chat.c
  interactive diagnostic console and future runtime-backed REPL shell
```

## Code-First Delivery Shape

A delivery should usually contain:

```text
1. implementation in owner files
2. tests proving the behavior
3. negative/boundary tests
4. claim guards when capability text changes
5. minimal docs update recording the implemented state
```

Documentation-only delivery is allowed only when the user explicitly asks for
docs or when an implementation would be unsafe without first defining a narrow
contract.

## Tests Are Implementation

A delivery is incomplete without tests for the new behavior.

For runtime/backend/kernel work, tests must include:

```text
reference result
implementation result
numeric tolerance where needed
edge case
failure or refusal path
no benchmark disguised as test
```

For source/model work, tests must include:

```text
missing source
fake source
malformed input where relevant
normal/table/audit preservation
no payload loading where relevant
no readiness claim
```

For CLI work, tests must include:

```text
normal output
audit output
parser failure
exit code
compactness if output surface is involved
```

## Output Rules

Default operator output should be compact.

Use:

```text
normal
table
audit
```

where already supported.

Rules:

```text
normal:
  short human-readable output

table:
  aligned plain-text rows for list-like commands

audit:
  complete diagnostic/evidence fields
```

Do not dump audit fields into normal output.

Do not add JSON, color, or metric output unless actually implemented and tested.

## Boundaries and Claims

Never claim a capability without implementation, tests, command proof, and
documented boundary.

Forbidden unless actually true:

```text
full model support
inference ready
generation ready
prefill ready
decode ready
CUDA runtime ready
Metal runtime ready
Qwen supported
Gemma supported
DeepSeek generation implemented
benchmark measured
throughput achieved
release_ready: true
execution_ready: true
generation_ready: true
```

Use precise lower-scope language:

```text
report-only
header-only
diagnostic-runtime
selected-slice
bounded primitive
fixture-proof
source-pressure
target-profile
unsupported
not-measured
```

If the implementation is a primitive, call it a primitive.

If it is a report, call it a report.

If it is diagnostic, call it diagnostic.

## Artifact Guardrail

Do not commit model artifacts or local operator state.

Forbidden:

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

Tracked `.gguf` files must be tiny test fixtures only.

External model artifacts belong outside git and may be documented only as
operator-local.

Do not publish personal absolute artifact paths in public docs.

## Backend Work

Backend work must be vertical.

A backend delivery should include:

```text
device/kernel implementation
host launch or binding
reference test
edge guard
claim guard
minimal docs update
```

Do not add a backend manifesto before implementation.

CUDA work must preserve the existing CUDA Driver API style unless the user
explicitly approves a change.

Metal work should follow the same discipline:

```text
Metal primitive
host path
reference test
edge guard
no runtime/generation claim
```

## Source and Tensor Work

Source/tensor work must preserve the payload boundary unless the row explicitly
owns payload loading.

Header-only means header-only.

Do not read tensor payload bytes during source inventory, metadata inventory,
provenance reporting, or native safetensors header inventory.

Do not infer runtime roles from lexical tensor names unless the row explicitly
owns tensor mapping.

Lexical name-pattern summaries are not tensor role mapping.

Metadata inventory is not model-class proof.

Native inventory is not runtime readiness.

## Error Handling

Prefer precise failure classes over vague errors.

Good:

```text
missing-source-path
unsupported-output-mode
malformed-safetensors-header
source-payload-not-loaded
unsupported-family
invalid-release
```

Bad:

```text
failed
bad input
not ready
error
```

Parser failures should return the established parser failure exit code.

Do not silently accept unsupported modes or unknown flags.

## Documentation Rules

Documentation updates must be minimal and implementation-backed.

Update docs only to record:

```text
what is now true
how to prove it
what boundary remains
what the next real implementation blocker is
```

Do not expand roadmap prose unless needed to reconcile current implementation
state.

Do not add docs sprawl.

Canonical docs:

```text
docs/spine.md
docs/runbooks/common.md
docs/model-families.md
docs/api.md
docs/contract.md
docs/operator-runbook.md
MODEL_ARTIFACTS.md
```

Do not create duplicate docs for the same concept.

## Validation

Baseline validation:

```sh
git diff --check
make
make smoke
make check
```

CUDA-capable hosts:

```sh
make cuda
make test-cuda
make check-cuda
```

Surface and hygiene checks:

```sh
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
```

Artifact guardrail:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected:

```text
no tracked large model artifacts
tracked GGUF files are tiny test fixtures only
```

## Review Failure Conditions

A delivery fails review if it:

```text
adds behavior to the wrong owner module
creates new source files without a real ownership need
adds docs without implementation
adds capability claims without executable proof
commits model artifacts or local state
moves detailed behavior into yvex_cli.c
adds large hardcoded output walls to normal output
duplicates existing state fields instead of consolidating them
introduces a new abstraction that does not remove real complexity
breaks normal/table/audit behavior
reads tensor payloads in a header-only/source-report row
claims runtime/generation/benchmark readiness without proof
```

## Final Principle

Every patch should leave YVEX more real.

```text
less roadmap
less placeholder
less duplicate state
less doc-driven work

more executable code
more owner-file coherence
more reference tests
more bounded primitives
more honest boundaries
```
