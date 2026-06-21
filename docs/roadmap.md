# YVEX Roadmap

This is the working progress document for YVEX. `docs/spine.md` remains the
technical authority; this file owns milestone state, delivery sequence,
acceptance gates, decisions, and handoff to the next implementation wave.

## Current Repo Status

```text
phase: C0 artifact and GGUF parser base
head at A0.1 intake: 164ec95
interface: CLI-only
implemented runtime: core version/status/error/log, runtime filesystem paths/run directories, artifact byte views, GGUF header/probe, CLI bootstrap
not implemented: GGUF metadata, tensor directory, tokenizer, backend, CUDA, session, server, inference, TUI
next milestone after C0: C1 GGUF metadata and tensor directory
```

Current build surface:

```text
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h
include/yvex/fs.h
include/yvex/artifact.h
include/yvex/gguf.h
src/core/version.c
src/core/status.c
src/core/error.c
src/core/log.c
src/fs/paths.c
src/fs/run_dir.c
src/artifact/artifact.c
src/artifact/range.c
src/formats/gguf.c
cli/yvex_cli.c
tests/test_status.c
tests/test_error.c
tests/test_version.c
tests/test_log.c
tests/test_fs.c
tests/test_artifact.c
tests/test_gguf.c
tests/test_cli.sh
```

Current documentation surface:

```text
docs/README.md
docs/spine.md
docs/roadmap.md
docs/api.md
docs/backend-contract.md
docs/runtime-filesystem.md
docs/cli-runtime.md
```

## Completed Milestones

| ID | Status | Commit | Result |
|---|---|---|---|
| P0 | complete | b7c3dcf | Established YVEX pre-implementation spine. |
| P0.6 | complete | f2e4890 | Purged legacy scaffold surface. |
| P0.7 | complete | 70e8837 | Cut over remote identity to YVEX. |
| P0.8 | complete | bbef021 | Defined runtime system design for A0. |
| A0 | complete | 2620c59 | Added core C skeleton and CLI bootstrap. |
| A0.1 | complete | 164ec95 | Hardened core skeleton style and CLI command contract. |
| A0.2 | complete | 7e5879c | Consolidated docs, refounded roadmap, and ran code-quality gate. |
| B0 | complete | 8e7d6c8 | Added runtime filesystem paths, project-local mode, run-directory skeleton, CLI proof, and tests. |
| C0 | complete | current commit | Added artifact byte view, range checks, GGUF header/probe parser, inspect command, fixtures, and tests. |

## Exact Delivery Sequence

```text
A0.2  Documentation consolidation / roadmap refoundation / code quality gate
B0    Runtime filesystem
C0    Artifact and GGUF parser base
C1    GGUF metadata and tensor directory
D0    Tensor and model layer
E0    Tokenizer and prompt rendering
F0    Graph and planner
G0    CPU reference backend
H0    Engine and session runtime
I0    CLI run/chat runtime
J0    Metrics and tracing
K0    yvexd server/provider
L0    CUDA/DGX Spark backend
M0-M8 model support ladder
```

No milestone may be renamed, split, reordered, or renumbered without updating
this roadmap and `docs/spine.md` in the same change.

## A0.2 Acceptance Criteria

A0.2 is accepted only when:

```text
docs/ contains only README.md, spine.md, roadmap.md, api.md,
  backend-contract.md, runtime-filesystem.md, and cli-runtime.md
docs/README.md lists only those canonical documents
docs/spine.md contains the absorbed technical doctrine
docs/roadmap.md contains this progress/gate record
docs/api.md marks future API sections as not implemented
docs/backend-contract.md owns CUDA/DGX Spark planning
docs/cli-runtime.md owns layout and JSON/JSONL policy
Makefile check-docs requires only the remaining docs
make clean passes
make check passes
make smoke passes
git diff --check is clean
no future public headers are introduced
```

## Current A0.1 Code Review

Acceptable:

```text
include/yvex/yvex.h aggregates only implemented core headers
status.h has a compact enum and deterministic name/predicate helpers
error.h uses caller-owned fixed-size error objects and accessor helpers
log.h is restricted to CORE and CLI domains
version.c has no allocation, environment reads, or runtime discovery
status.c maps every current status code
error.c handles null pointers and fixed buffers without heap/log/abort
CLI command table exposes only implemented commands
yvex info honestly reports inference, GGUF, CUDA, and server as not implemented
```

Must improve before or during B0:

```text
version.h wording must describe core version constants, not stable A0.1 status
error.c buffer capacities should use size_t, not unsigned long
yvex_status_is_error treats any non-OK value, including unknown positive values,
  as an error; this is intentional and must remain tested or documented
Makefile documentation checks must enforce the reduced docs tree
B0 path work must add tests before claiming filesystem behavior
```

Must not expand yet:

```text
no GGUF/model/backend/session/server public headers
no TUI path, option, or dependency
no future CLI command listed as implemented
no CUDA build target that claims backend support
no provider/server work before the runtime foundation exists
```

## B0 Pass/Fail Criteria

B0 passes only if it adds runtime filesystem behavior without crossing into
model execution.

Required B0 outputs:

```text
XDG config/cache/state resolution
project-local .yvex resolution
explicit argument > env > project-local > user default precedence
run directory creation
lock/cache policy documented
filesystem tests
CLI paths smoke tests
no model downloads
no CUDA requirement
no YAI checkout requirement
```

## C0 Pass/Fail Criteria

C0 may start only after B0 validation passes. C0 must add artifact and GGUF
header/probe parsing without claiming model execution.

Required C0 outputs:

```text
artifact open/stat path
bounded byte cursor or equivalent checked read path
GGUF header parsing
malformed header fixtures
inspect command only when backed by parser code
metadata and tensor directory explicitly deferred
no tokenizer claim
no CUDA claim
no model execution claim
```

## C1 Pass/Fail Criteria

C1 may start only after C0 validation passes. C1 must extend GGUF parsing into
metadata and tensor directory handling without claiming model execution.

Required C1 outputs:

```text
metadata key/value table for supported scalar/string/array cases
tensor directory parser
alignment and tensor offset checks
malformed metadata/tensor fixtures
inspect metadata/tensors output if backed by parser code
no tokenizer claim
no backend claim
no inference claim
```

## Do Not Proceed Gates

Stop before B0 if any of these are true:

```text
docs/ contains extra Markdown not listed in docs/README.md
make check fails
make smoke fails
future public headers appear without implementation
CLI help lists unimplemented runtime commands as implemented
README.md claims inference, CUDA, server, or model support
TUI files, options, dependencies, panels, or dashboards appear
```

Stop before C0 if any of these are true:

```text
make check fails
make smoke fails
build/tests/test_fs fails
yvex paths --run --create cannot create a controlled run directory
filesystem APIs are absent from docs/api.md
runtime-filesystem.md claims config parsing or artifact writing is implemented
```

Stop before C1 if any of these are true:

```text
make check fails
make smoke fails
build/tests/test_artifact fails
build/tests/test_gguf fails
yvex inspect valid-minimal.gguf does not report status: header-only
bad GGUF magic crashes or reports model loading behavior
docs/api.md omits artifact or GGUF header/probe APIs
```

## Decision Log

| Date | Decision |
|---|---|
| 2026-06-19 | YVEX remains CLI-only; no TUI track exists. |
| 2026-06-19 | A0.1 code is accepted as a small honest core base. |
| 2026-06-19 | B0 is deferred until A0.2 reduces documentation sprawl. |
| 2026-06-19 | `docs/spine.md` is the technical authority and this roadmap is the working progress document. |
| 2026-06-19 | Future APIs stay in documentation until headers, implementation, tests, and CLI-visible behavior exist. |
| 2026-06-21 | B0 adds only filesystem paths and run-directory creation; config parsing and run artifact writing stay deferred. |
| 2026-06-21 | C0 adds only artifact bytes and GGUF header/probe; metadata, tensor directory, tokenizer, and model loading stay deferred. |

## Delivery Format

Each wave report must state:

```text
ID
goal
scope
files changed
non-goals preserved
validation commands
validation results
remaining deferred work
next authorized milestone
```
