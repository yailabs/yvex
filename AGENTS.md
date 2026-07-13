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

src/cli/model_artifacts/
  CLI-only command-family surface owners for historical models, download,
  prepare/check, fullmodel, attention, context, MoE, and tensor-collection
  behavior while the model-artifacts command surface continues moving toward
  fully typed input/report/render ownership; never libyvex ownership

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
  model-target command dispatch only; no target facts, profile specs, tensor
  maps, qtype policy, sidecar writing, or report construction

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
  graph construction and graph state only

src/graph/yvex_memory_plan.c
  graph memory plan construction and memory facts

src/graph/yvex_graph_plan.c
  graph execution plan construction and backend capability facts

src/graph/yvex_graph_report.c
  graph report construction and explicit FILE dump compatibility

src/graph/yvex_graph_guard.c
  graph guard facts for selected graph slices

src/graph/yvex_graph_primitive.c
  graph primitive fixture/proof facts and reference comparisons

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
  compatibility anchor only after MODEL_ARTIFACTS.0; no command, render,
  registry, reference, gate, or writer ownership

src/model/artifacts/
  model artifact registry storage, model reference resolution, model gates,
  typed model artifact reports, status/list/check report builders, and explicit
  local registry/report file writing

src/model/target/
  model-target catalogs, target decisions, candidate facts, model-class
  profiles, tensor collection reports, tensor naming reports, output-head maps,
  tokenizer maps, missing-role reports, mapping gates, qtype policy reports,
  qtype role-support reports, model-target report construction, and explicit
  sidecar file writing

src/model/architecture/
  immutable typed family architecture specifications constructed from verified
  source facts; no source parsing, tensor-name discovery, mapping, artifact,
  runtime, or rendering ownership

V010.MODEL.ARCH.IR.0 makes
src/model/architecture/yvex_deepseek_v4_ir.[ch] the sole owner of the exact
DeepSeek-V4-Flash model, main-layer, auxiliary/MTP, attention, position, KV,
mHC, MoE, output, tokenizer, and source-constraint specification. It consumes
one successful strict source-verification result without reopening source
files or rescanning headers. The immutable IR defines required semantics for
tensor coverage and later runtime work; it is not tensor mapping, artifact
support, materialization, execution, or generation.

V010.TENSOR.COVERAGE.DEEPSEEK.0 makes
src/model/target/yvex_deepseek_tensor_coverage.[ch] the sole owner of the exact
DeepSeek source tensor requirement set and one-to-one coverage result. It
consumes the immutable architecture IR and the retained verified source tensor
snapshot, validates name, collection, scope, layer/expert identity, rank,
shape, dtype, and quantization companions, and refuses every missing,
duplicate, ambiguous, unsupported, or unexpected entry. It may not reopen
source files, rescan headers, define GGUF names/transforms, read payloads, or
promote artifact/runtime support. Tensor collection, missing-role, and mapping
gate reports project this result rather than reclassifying DeepSeek names.

MODEL_TARGET.1 closure forbids using yvex_model_target_report.c as a
compatibility monolith. The coordinator may route request kinds, but
report-specific ownership must live in the corresponding src/model/target
module. Boundary typedef shells and captured-output report buffers are
forbidden.

MODEL_TARGET.2 closure forbids yvex_model_target_internal.c as a compatibility
backend. Specialized model-target modules must own their own report
construction. Shared utilities may exist only as small pure helper modules
without report-specific logic, output buffers, sink streams, sidecar writing, or
FILE-based output.

MODEL_TARGET.3 is the hard model-target closure row. It forbids shared
runner/internal/compatibility backends, CLI-shaped request fields in domain
report APIs, and pre-rendered report text buffers. Model-target report modules
must own typed facts directly, and CLI renderers must render typed reports
rather than captured command output.

TOPOLOGY.CELL.CLOSURE.1 is audit-only. It must inventory residual
control-plane/domain drift before cleanup. Audit findings must distinguish
allowed CLI IO, allowed explicit file writers, legacy pending residue, and hard
violations. Closure cannot be claimed while direct output, CLI-shaped input,
renderer/domain leakage, compatibility shells, or libyvex CLI leakage remain
unresolved.

MODEL_ARTIFACTS.0 closes the critical model-artifacts topology residue. Model
artifact registry, references, gates, reports, explicit file writing, CLI input,
command dispatch, and rendering must live in separate ownership modules.
src/model/yvex_model_artifacts.c may not contain operator output, CLI parsing,
CLI/operator includes, rendering, or broad command-surface ownership.

MODEL_ARTIFACTS.1 forbids using
src/cli/commands/yvex_model_artifacts_cli.c as a command-surface monolith.
Model-artifacts public command symbols must stay thin adapters. Historical
models/fullmodel/attention/context/moe and tensor-collection surface code may
live only under src/cli/model_artifacts as CLI-only transitional residue until
typed input/report/render ownership is completed; it must not enter libyvex.a.

MODEL_ARTIFACTS.2 forbids
src/cli/model_artifacts/yvex_model_artifacts_surface.c from acting as a
historical CLI surface monolith. Historical models, download, prepare/check,
fullmodel, attention, context, MoE, and tensor-collection command families must
be split into command-family surface owners. Shared CLI-only helpers must remain
small and may not become a compatibility backend.

MODEL_ARTIFACTS.3 makes src/cli/model_artifacts a surface-routing layer only.
Model-artifacts command-family surfaces may route commands and call render
owners, but they may not format normal/table/audit/help/JSON output, parse argv
manually, or own domain facts. Historical output compatibility belongs under
src/cli/render until each family is converted to typed report facts.

SPINE.SYSTEM.TARGET.0 defines the target filesystem as an enforceable
architecture contract. Missing target owner files may be installed only with
real owner contracts, typed boundary/refusal behavior, and no capability
claims. Empty placeholders, compile-only anchors, runner/internal compatibility
backends, and broad monolithic replacement files are forbidden. GGUF ABI, qtype
geometry, writer, roundtrip, artifact descriptor, materialization, runtime
descriptor, graph bind, and backend qtype owners must remain distinct.

V010.GGUF.ARTIFACT.ABI.0 closes only the GGUF container, metadata,
tensor_info, and ABI-visible range boundary. It may validate magic, version,
metadata key/value ABI, tensor_info ABI, and byte-range safety using tiny
fixtures. It must not implement writer completion, qtype compute support,
quantization, roundtrip completion, generation-capable artifact emission,
materialization proof, runtime descriptor readiness, graph execution, or
generation.

V010.GGUF.QTYPE.ABI.0 closes only GGUF qtype byte geometry: qtype id,
canonical name, storage class, block size, bytes per block, storage-size
calculation, and explicit refusal for unknown or ambiguous qtypes. It must not
claim backend arithmetic capability, CUDA arithmetic capability, qtype policy
selection, quantization, writer completion, roundtrip, artifact emission,
materialization, runtime descriptor readiness, graph execution, or generation.

V010.GGUF.QTYPE.ABI.1 makes `include/yvex/gguf_qtype.h` and
`src/gguf/yvex_gguf_qtype.c` the sole GGUF qtype identity and storage-geometry
owner. Block storage is calculated from the complete shape: `ne[0]` is the row
width, every row must divide exactly by the canonical block width, and the
remaining dimensions form the checked row count. Dtype, parser, range,
integrity, conversion, and memory-plan code may project these facts but may not
copy IDs, names, block widths, or block bytes. Storage admission remains
independent from reference decoding, quantization, emission, backend compute,
materialization, and runtime support.

V010.GGUF.ARTIFACT.ABI.1 makes the read-only artifact handle and native GGUF
v3 reader the structural authority. Structural parsing uses bounded positioned
reads, 64-bit file offsets, configured resource budgets, indexed duplicate
detection, typed section/code/offset/index failures, and an immutable owned
view. It must read zero tensor payload bytes. Reports, inspect, metadata,
tensors, and integrity preflight project that view; they may not reparse or
classify failures from error text. Global tensor order, padding, overlap, and
aggregate layout admission remain owned by V010.GGUF.LAYOUT.INTEGRITY.1.

V010.GGUF.LAYOUT.INTEGRITY.1 makes `include/yvex/gguf_layout.h` and
`src/gguf/yvex_gguf_layout_integrity.c` the sole global GGUF layout-admission
owner. Alignment must be nonzero and power-of-two. Directory order must equal
physical order, the first relative tensor offset must be zero, and every next
offset must equal the preceding qtype-sized padded end. Required directory and
tensor padding must be zero. Integrity, materialization, graph, runtime, and
report owners consume the typed result; they may not independently sort,
recalculate, or infer global layout from local ranges. Layout validation reads
padding only and zero tensor payload bytes. Layout admission is not complete
model-artifact support, payload trust, materialization, or runtime support.

src/artifact/yvex_artifact.c
  read-only file handles, explicit mapping, positioned artifact IO, inspect,
  metadata, and tensor command surfaces

src/artifact/yvex_artifact_identity.c
  artifact identity and digest behavior

src/artifact/yvex_artifact_integrity.c
  artifact integrity, corruption/refusal reports

src/gguf/
  file-backed GGUF structural parsing, typed reader failures and budgets,
  immutable parsed views, conversion, and quant/intake internals

src/gguf/tools.c
  GGUF tooling command surfaces

src/gguf/conversion.c
  selected tensor conversion bridge and mapping helpers

src/gguf/quant.c
  quantization manifest and policy internals

src/source/yvex_source.c
  source manifests, source pressure, source evidence, native header inventory

src/source/yvex_source_json.c
  bounded structured JSON primitives only; no source policy

src/source/yvex_source_deepseek.c
  raw DeepSeek config, tokenizer, and generation sidecar facts

src/source/yvex_source_provenance.c
  pinned repository/revision, provider metadata, and source-manifest facts

src/source/yvex_source_inventory.c
  upstream-indexed or explicitly header-derived shard inventory and the single
  canonical safetensors header pass; it publishes the retained immutable tensor
  snapshot consumed by model-target coverage without source rescanning

src/source/yvex_source_verify.c
  exact release-source verification coordination, final policy, blockers, and
  verifier-controlled manifest promotion; never JSON, shard/header, rendering,
  serialization, tensor payload, or model-support ownership

src/source/yvex_source_write.c
  explicit source sidecar serialization and atomic verified-manifest or
  header-derived-inventory publication; never verification policy or operator IO

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
  bounded sampling over logits

src/generation/yvex_sampling_report.c
  sampling report construction and runtime/report orchestration

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

Canonical cell network:

```text
CLI entry:
  src/cli/yvex_cli.c

Input:
  src/cli/input/yvex_<surface>_args.c
  parses argc/argv into typed args only.

Command:
  src/cli/commands/yvex_<surface>_cli.c
  dispatches only: parse args, build request, call domain/report API, call renderer,
  return exit code.

Domain:
  src/<domain>/*.c
  owns state, algorithms, validation, lifecycle, backend calls, and facts.

Report:
  src/<domain>/yvex_<surface>_report.c
  builds typed report structs from domain facts.

Render:
  src/cli/render/yvex_<surface>_render.c
  formats typed reports into normal/table/audit/json-compatible output.

CLI IO:
  src/cli/io/*
  is the only stdout/stderr byte writer.

File IO:
  src/io/* or explicit domain writer files
  writes explicit local files only and never stdout/stderr.

Primitive/reference:
  backend primitives execute operations.
  reference primitives compute expected outputs.
  domain/report compares primitive output and reference output.
  render only prints the comparison report.
```

Graph primitive proof is a domain/report fact, not CLI output. Backend primitive
output and reference output must be compared below the render layer. CLI render
may print checksum, tolerance, status, and boundary, but must not execute
primitives or compute references.

Model-target cell closure requires target catalogs, target decisions,
model-class profiles, tensor collection reports, tensor naming reports,
output-head maps, tokenizer maps, missing-role reports, mapping gates, qtype
policy reports, role-support reports, and sidecar writing to live under
src/model/target. The CLI command adapter may only parse, dispatch, render, and
return exit codes through typed APIs.

A command adapter that does not call a real input parser, a real domain/report API,
and a real typed renderer is not a valid cell file.

An empty command adapter is forbidden.

A render file that does not render a typed report is forbidden.

A domain file that prints operator output is forbidden.

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

A closed cell must not leave its command/help/parser/render/output code in the
domain module.

Generation cell closure specifically forbids yvex_generate_command,
yvex_generate_help, argv parsing, usage text, direct stdio, and CLI/operator
includes under src/generation.

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

`PROJECT.md` is the single living engineering control file. Agents update it at
every milestone that changes executable state, closes a hard dependency, or
changes the truthful project-control contract. It owns the product target,
current hard truth, stable architecture-track registry, complete wave ledger,
milestone state, dependencies, family scope, evidence classification, release
gates, calculated counts, and Active Next.

The project control file has no arbitrary line or heading-count limit. Control
comes from explicit ownership, traceable architecture responsibilities,
conclusive milestones, current usefulness, and non-duplication.

Git history is recovery evidence, not a substitute for the current ledger. A
wave ID may not silently disappear, change owner, change rank, or be merged.
Record an explicit migration and successor. Project-control guards must verify
unique IDs, valid rank/state vocabulary, per-track coverage, and exactly one
active milestone.

Rank and state are separate. Only a `milestone` row may become Active Next or
close its explicitly owned gate. `capability`, `evidence`, `subtask`,
`migration`, and `future` rows remain visible under their stable track and must
name the conclusive consumer or future scope decision that owns them.

Tracks are architecture ownership lanes. Diagnostic commands, reports,
fixtures, selected tensor proofs, audits, and transcripts are evidence attached
to an owning milestone; they are not tracks or independent product milestones.
A milestone closes only a complete architectural/functional slice or an
explicit hard governance dependency. Documentation-only closure never promotes
artifact, runtime, generation, evaluation, benchmark, or release capability.

DeepSeek-V4-Flash is the sole v0.1.0 release target. It is not the sole
engineering scope. Existing Qwen, Gemma, dense/common, MoE, fixture, topology,
and selected-proof work remains in the project ledger at its lowest true rank.
Common source, map, qtype, GGUF, integrity, materialization, descriptor,
backend, operator, evaluation, and release owners must remain family-aware and
must not hard-code the release target.

Temporary or parallel project-control spines are forbidden. Put milestone
state, dependency order, decommission obligations, calculated counts, and
Active Next only in `PROJECT.md`. Permanent technical documents may link to
that authority but may not repeat its current state.

Canonical artifact terminology:

```text
tensor proof artifact
  one tensor or a bounded tensor subset

complete model artifact
  every tensor and metadata item required to execute the exact model

supported model artifact
  complete artifact that passes integrity, materialization, runtime,
  generation, evaluation, benchmark, and release gates
```

Do not use unqualified "model artifact" for a selected-tensor proof file.

Canonical docs:

```text
PROJECT.md
docs/system-target.md
docs/v010-release-doctrine.md
docs/model-families.md
docs/api.md
docs/contract.md
docs/operator-runbook.md
docs/cli-output-architecture.md
docs/reference-architecture.md
MODEL_ARTIFACTS.md
AGENTS.md
```

Documentation ownership is non-overlapping:

```text
PROJECT.md
  project state and complete ledger

docs/system-target.md
  filesystem and module topology

docs/model-families.md
  family integration architecture

MODEL_ARTIFACTS.md
  GGUF and artifact terminology, admission, and lifecycle

docs/api.md and docs/contract.md
  public API and implemented behavior contracts

docs/operator-runbook.md and docs/runbooks/
  executable operator procedures

docs/topology-closure-audit.md
  point-in-time evidence

docs/reference-architecture.md
  papers, specifications, external implementations, and YVEX owner mapping
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
