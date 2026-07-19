# AGENTS.md

## 0. Repository contract

YVEX is a native C/CUDA inference system for verified open-weight model
artifacts. Every patch must make the repository more executable, more tested,
or more internally coherent. Documentation records implemented truth; it does
not create capability.

Work in this order unless the delivery explicitly owns a doctrine boundary:

1. code;
2. tests;
3. project control and documentation.

Never commit model weights, generated complete artifacts, local registries,
provider credentials, build output, reports, or downloaded dependencies.

## 1. Directory is the namespace

The path identifies the source namespace. Owned filenames do not repeat the
project, their immediate directory, or their complete ownership hierarchy.

Hard rules, enforced by `tests/test_repository_layout.sh`:

- implementation lives under `src/`;
- installed public headers live under `include/yvex/`;
- tests live under `tests/` and never enter production objects;
- filenames use lowercase snake_case;
- owned filenames never begin with `yvex_`;
- basenames are at most 32 characters including the extension;
- paths are at most five components below the repository root;
- `_internal.c`, `_private.c`, and mechanically paired private files are
  forbidden;
- a basename does not repeat its immediate directory token;
- root C sources and root private headers are forbidden;
- source-relative object paths are mandatory, for example
  `build/obj/src/graph/plan.o`;
- static-archive members preserve those source-relative identities; duplicate
  member basenames are not an acceptable approximation;
- build logic may not flatten objects or rely on long prefixes to avoid
  collisions.

The installed namespace directory `include/yvex/`, exported `yvex_` symbols,
the `yvex` and `yvexd` executables, standard root documents, and external ABI
filenames are the only categorical naming exceptions.

## 2. Semantic owner admission

A new file is not an implementation convenience. It is a new semantic owner
or independently compiled boundary.

A production source may exist only when it owns at least one of:

1. a public or subsystem ABI;
2. an independent resource lifecycle;
3. a distinct reusable algorithm with multiple production consumers;
4. an independently compiled platform or backend implementation;
5. reproducibly generated code or ABI;
6. one admitted family descriptor or recipe within the family budget;
7. a necessary executable or daemon entrypoint.

Otherwise extend the existing canonical owner, make helpers `static`, or merge
the fragment. File size, development chronology, one differently named
function, or hypothetical future reuse is not an owner boundary.

A private header may survive only when at least two production translation
units consume it or when it is a required backend/platform interface. Each
subsystem has at most one general `private.h`. One-consumer declaration shells
belong in the source owner. Public headers require a real installed ABI.
Mechanical same-stem `.c`/`.h` pairing, forwarding headers, and compatibility
headers for obsolete in-tree paths are forbidden.

Explicit user authorization is required before:

- exceeding a family budget;
- adding another file to an existing semantic owner;
- adding a layout or ownership exception;
- introducing a public header;
- introducing a subsystem.

## 3. Machine-readable ownership

`config/source_owners.tsv` is the canonical source-ownership manifest. Every
owned `.c`, `.h`, and `.cu` under `src/` and `include/` appears exactly once.
Its tab-separated fields are:

```text
path
subsystem
semantic_owner
scope
visibility
boundary
primary_consumers
partition
exception_id
```

Allowed scopes are `generic`, `family`, `backend`, and `entrypoint`. Allowed
visibility is `public` or `private`. An owner may have one `interface` and one
`implementation` partition; backend kernels and entrypoints use their named
partitions. Exceptions are forbidden unless explicitly authorized and given a
stable identifier.

Before adding or moving production code:

1. identify the canonical owner and consumers;
2. prove the admission reason above;
3. update the manifest in the same patch;
4. update build inputs without flattening objects;
5. add or move focused tests with the owner;
6. run the ownership, layout, and dependency guards.

`tests/test_source_ownership.sh` rejects missing and duplicate registrations,
fragmented owners, invalid private headers, family scope/path mismatch, family
phase files, and family-budget violations. The manifest cannot be used to
legitimize arbitrary fragmentation.

## 4. Generic and family boundaries

Generic owners implement reusable mechanisms. Family owners select facts,
policies, schedules, and operation composition; they do not clone generic
mechanisms.

A new model family defaults to:

```text
src/model/families/<family>.c
```

An irreducible graph recipe may add:

```text
src/graph/families/<family>.c
```

A genuinely fused backend implementation may add:

```text
src/backend/<backend>/families/<family>.<ext>
```

Without explicit user authorization, one family has a maximum of three production
sources repository-wide, one per subsystem, and one optional header
repository-wide. Family-specific `plan`, `execute`, `sink`, `numeric`,
`internal`, `reference`, and `report` files are forbidden.

Generic attention owns the immutable history envelope, state transaction,
sink protocol, Hadamard, activation quantization, scale codecs, deterministic
top-k, masks, softmax, reductions, encoded matrix primitives, and backend
admission. A family owns only irreducible scheduling, tensor-role lowering,
recurrence, operation composition, and numeric-policy selection.

## 5. C interfaces, symbols, and contracts

Headers have exactly three tiers:

- `include/yvex/*.h` is installed public ABI grouped by stable domain;
- `include/yvex/internal/*.h` is non-installed cross-subsystem ABI;
- a repository-qualified `src/<subsystem>/private.h` is source-local ABI shared
  by several translation units in that subtree.

`include/yvex/api.h` is an external convenience umbrella. Production code
never includes it. Public headers include only public headers; they are
self-contained in both C and C++. Internal headers are absent from the
umbrella and never include source-local headers. Source code names every
dependency explicitly with `<yvex/domain.h>`, `<yvex/internal/domain.h>`, or a
repository-qualified path such as `"src/graph/private.h"`. Bare internal
includes such as `"private.h"` or `"report.h"` are forbidden; include-path
ordering is not a dependency mechanism.

Exported symbols retain the `yvex_` namespace and use
`yvex_<subsystem>_<operation>`. A family exposes one bounded registration or
lowering entrypoint per applicable subsystem. Private functions are `static`
and do not use `yvex_`. A non-public global requires a declared internal ABI
and more than one production translation-unit consumer. Do not export helpers
merely to connect artificially split files or promote diagnostics, fixtures,
references, renderers, or family implementation types into installed ABI.

Every source file declares these fields in its leading ownership contract:

```text
Owner
Owns
Does not own
Invariants
Boundary
Purpose
Inputs
Effects
Failure
```

Every production function has an adjacent semantic contract. Stateful,
allocating, I/O, identity, lifecycle, capability, transactional, and other
non-trivial functions state `Purpose`, `Inputs`, `Effects`, `Failure`, and
`Boundary`. A tiny pure helper may use one concise `Purpose` statement. The
comment must name the actual invariant or transformation; generated manifest
prose, function-name paraphrases, and repeated boilerplate are rejected.

`.clang-format` is the canonical visual style: 100-column target and 120-column
hard limit. Production translation units stay at or below 2,000 physical
lines, headers at or below 600, and functions at or below 200. Required warning
flags are recorded once in `config/c_policy.json` and enabled by the Makefile;
warnings are fixed at their owner rather than globally suppressed. Code is not
made shorter through macros, one-line statement compression, hidden generated
implementation, or opaque callback dispatch.

Use checked allocation and arithmetic, the existing typed failure style, and
one canonical field for each fact. Do not hash C object memory, pointers,
padding, local paths, or timestamps into semantic identities.

## 6. Dependency DAG

The intended dependency direction is:

```text
core / public ABI
  -> source, GGUF, artifact, model, tokenizer
  -> compilation and graph planning
  -> materialization and runtime
  -> backend execution
  -> generation
  -> evaluation and benchmark

domain facts -> typed reports -> CLI render -> CLI I/O
```

Hard prohibitions, enforced by `tests/test_architecture_boundaries.sh`:

- core does not depend on CLI or render;
- model, graph, runtime, and generation do not depend on CLI;
- generic graph and numeric owners do not depend on family implementations;
- model planning does not depend on backend implementations or read payload
  bytes;
- backend code does not reconstruct model topology;
- production never includes test code;
- tests never become production objects;
- report and render code do not become capability authorities;
- runtime cannot bypass artifact admission or materialization identity;
- family code cannot create a second qtype, dtype, failure, or identity
  registry;
- cross-subsystem include cycles are forbidden;
- duplicate globally exported symbols are forbidden.

Cross-owner access uses the smallest typed ABI. A subsystem does not include
another subsystem's private header. Tests use public or explicit test ABIs;
private headers are allowed only for focused invariant tests that cannot be
expressed through an admitted public boundary.

## 7. Canonical capability ownership

The following boundaries remain distinct:

- `TRACK.SOURCE` owns source identity, retained inventories, payload trust,
  ranges, bounded reads, and transactional delivery;
- `TRACK.COMPILATION` owns artifact-neutral transformation semantics,
  derivation identity, physical-variant planning, and immutable bindings;
- GGUF owners under `src/gguf/` own container ABI, qtype geometry, codecs,
  writer planning, and structural admission according to their typed APIs;
- artifact owners under `src/artifact/` own artifact snapshot identity,
  complete-artifact admission, materialization, and lifecycle;
- model families under `src/model/families/` own family facts, requirement
  coverage, and family lowering composition;
- graph owners under `src/graph/` own graph state, execution protocols,
  reusable numerical operations, and family graph recipes;
- backend owners execute admitted operations but do not infer model policy;
- generation owns prefill, persistent KV use, decode, logits, sampling, and
  loop integration only after their dependencies are executable.

Source intake is not payload trust. Mapping is not transformation execution.
Quantization is not a GGUF artifact. A complete artifact is not
materialization. Materialization is not graph execution. A primitive is not a
transformer. Transformer execution is not generation.

## 8. CLI, reports, and output

`src/cli/main.c` dispatches only. Input adapters parse typed arguments.
Commands call domain/report APIs and renderers. Renderers format typed facts.
Only `src/cli/io/` and the daemon entrypoint write operator stdout/stderr.
Explicit domain file serialization is allowed only for manifest-declared
`transactional-io` or `file-serialization` boundaries.

Domain code does not parse argv, own usage strings, render output, or classify
failure from text. CLI and render code do not own trust, model selection,
quantization policy, graph admission, runtime state, or capability decisions.

Supported operator modes are `normal`, `table`, and `audit` unless a typed
surface explicitly owns and tests JSON. Normal output stays compact; audit
output carries evidence. No mode may imply a higher capability stage.

## 9. Evidence and claim discipline

Use the lowest true evidence stage from the project ledger. A fixture, selected
slice, report, digest, artifact, primitive, or external runner cannot promote
the next boundary.

Do not claim full model support, runtime readiness, generation, evaluation,
benchmark results, or release readiness without implementation, focused tests,
failure proof, and an executable downstream consumer. DeepSeek-V4-Flash is the
v0.1.0 release target; it is not automatically supported by naming or artifact
presence. Qwen, Gemma, and other families remain at their independently proven
stages.

Canonical artifact terms:

- tensor proof artifact: one tensor or a bounded subset;
- complete model artifact: every required tensor and metadata item;
- supported model artifact: a complete artifact that passes materialization,
  runtime, generation, evaluation, benchmark, and release gates.

## 10. Tests and validation

Tests mirror semantic owners, not file fragments. Separate unit, integration,
live, fault, sanitizer, and external-compatibility tests only when their
harness or resource lifecycle differs materially.

Every behavior needs a positive test. Every refusal needs a failure test.
Numeric execution needs an independent reference, tolerance or exactness
contract, edge cases, cleanup, and backend comparison where applicable.

Minimum validation for repository work:

```sh
git diff --check
make
make smoke
make test-core
make check
make check
make check-docs
make check-guardrails
make test-cuda-no-nvcc
make check-cuda
tests/test_source_ownership.sh
tests/test_repository_layout.sh
tests/test_architecture_boundaries.sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Use ASan/LeakSanitizer and UBSan for changed ownership and lifecycle paths.
Run focused live evidence when the delivery touches a live owner. Two
consecutive builds/checks without cleaning are required after build topology
changes.

## 11. Project control and closure

`PROJECT.md` is the sole project-control authority. Stable milestone IDs do
not disappear or silently change owner. Rank and state remain distinct. There
is exactly one active milestone and exactly one Active Next.

A closure that changes ownership reports:

- owned production files before/after;
- source/header/test counts before/after;
- semantic owners and files per owner;
- basename and project-prefix violations;
- one-consumer private headers;
- exported symbols;
- family files and budget;
- include cycles and forbidden dependency edges;
- manifest, Makefile, and object-path parity;
- renamed, merged, and deleted files;
- net lines removed;
- preserved semantic identities and capability refusals.

No implementation may be declared complete because a report says so. Project
control changes only after executable evidence passes.

## 12. Final rule

Prefer fewer, stronger owners. Extend an existing owner unless an admitted
independent boundary is proven. Remove duplicate truth, forwarding shells, and
phase-shaped fragmentation. Preserve identity semantics and honest capability
boundaries.

Make YVEX more real.
