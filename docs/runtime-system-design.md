# YVEX Runtime System Design

Date: 2026-06-19
Status: P0.8 design contract
Applies to: A0 and immediate post-A0 implementation waves

## 0. Authority

This document defines the first executable runtime structure for YVEX. It does
not replace `docs/spine.md`; it narrows the spine into the initial
implementation system model.

`docs/spine.md` remains the canonical project spine.
`docs/runtime-system-design.md` defines the initial runtime skeleton that A0
must implement.
If this document conflicts with focused docs, `docs/spine.md` wins until the
focused docs are reconciled.

P0.8 does not write runtime C code. It defines the precise code boundary A0
will create.

## 1. Runtime Product Model

A0 products:

```text
libyvex.a
yvex
unit test binaries
```

Future products:

```text
yvexd
yvex_bench
CUDA backend object/library pieces
model fixtures
runtime run directories
```

Product roles:

```text
libyvex.a
  reusable core library linked by the CLI and tests

yvex
  CLI proof surface for implemented behavior

unit test binaries
  small C test programs linked against libyvex.a

yvexd
  future server binary, not produced in A0

yvex_bench
  future benchmark binary, not produced in A0
```

A0 must not produce `yvexd`, `yvex_bench`, CUDA objects, model fixtures, run
directories, or any model execution artifact.

## 2. A0 Source Tree Contract

A0 is allowed to create exactly this initial runtime surface:

```text
include/yvex/yvex.h
include/yvex/version.h
include/yvex/status.h
include/yvex/error.h
include/yvex/log.h

src/core/version.c
src/core/status.c
src/core/error.c
src/core/log.c

cli/yvex_cli.c

tests/test_status.c
tests/test_error.c
```

A0 may also create these files if the implementation needs them:

```text
tests/test_version.c
tests/test_log.c
tests/test.h
```

If `tests/test.h` is created, it must stay a tiny test helper and must be
reported in the A0 completion notes.

A0 must not create:

```text
include/yvex/artifact.h
include/yvex/gguf.h
include/yvex/tensor.h
include/yvex/model.h
include/yvex/backend.h
include/yvex/session.h
include/yvex/sampler.h
include/yvex/server.h
include/yvex/tui.h

src/artifact/
src/formats/
src/model/
src/tokenizer/
src/graph/
src/backend/
src/session/
src/server/
backends/
fixtures/
```

Rationale:

```text
A0 proves the build.
A0 proves public API discipline.
A0 proves CLI bootstrap.
A0 proves the C test harness.
A0 does not pretend artifact, GGUF, tokenizer, model, backend, session,
server, or provider support exists.
```

## 3. Header Layering

`version.h`:

```text
no YVEX header dependencies
exposes version constants
exposes version string/functions only
```

`status.h`:

```text
no YVEX header dependencies
exposes yvex_status
exposes status name/string helpers
```

`error.h`:

```text
depends on status.h
exposes yvex_error
exposes clear/set/query helpers
```

`log.h`:

```text
depends on status.h
may depend on error.h only for real formatting helpers
exposes log level and domain enums
exposes log level/domain name helpers
does not expose a complex global logger in A0
```

`yvex.h`:

```text
umbrella header
includes version/status/error/log
must not include future headers that do not exist
```

Forbidden in A0 public headers:

```text
CUDA types
POSIX file descriptors unless implemented and required
model structs
backend structs
session structs
fake opaque declarations for future APIs
terminal UI types
```

Headers are contracts. A0 headers may declare only implemented functions.

## 4. Core Module Dependency Graph

Initial dependencies:

```text
version
  no dependency

status
  no dependency

error
  depends on status

log
  depends on status
  may depend on error for real formatting helpers

cli
  depends on version, status, error, log

tests
  depend on public headers and libyvex.a
```

Dependency rule:

```text
A lower-level module must not call a higher-level module.
```

Forbidden:

```text
version.c including error.h
status.c including log.h
error.c depending on CLI
any core module depending on artifact/GGUF/backend/session modules
any core module depending on CUDA or server code
```

## 5. Version Contract

Minimum A0 API:

```c
#define YVEX_VERSION_MAJOR 0
#define YVEX_VERSION_MINOR 1
#define YVEX_VERSION_PATCH 0

const char *yvex_version_string(void);
int yvex_version_major(void);
int yvex_version_minor(void);
int yvex_version_patch(void);
```

Rules:

```text
version string is stable and non-null
version functions allocate nothing
version functions cannot fail
version API reads no files
version API reads no environment variables
version API is safe before any runtime init
```

CLI behavior:

```sh
./yvex --version
```

Expected output shape:

```text
yvex 0.1.0
```

## 6. Status Contract

Minimum A0 status enum:

```c
typedef enum {
    YVEX_OK = 0,
    YVEX_ERR = -1,
    YVEX_ERR_NOMEM = -2,
    YVEX_ERR_IO = -3,
    YVEX_ERR_FORMAT = -4,
    YVEX_ERR_UNSUPPORTED = -5,
    YVEX_ERR_BACKEND = -6,
    YVEX_ERR_BOUNDS = -7,
    YVEX_ERR_STATE = -8,
    YVEX_ERR_CANCELLED = -9,
    YVEX_ERR_INVALID_ARG = -10
} yvex_status;
```

Minimum A0 functions:

```c
const char *yvex_status_name(yvex_status status);
int yvex_status_is_ok(yvex_status status);
int yvex_status_is_error(yvex_status status);
```

Rules:

```text
YVEX_OK maps to "YVEX_OK"
unknown status maps to "YVEX_STATUS_UNKNOWN"
functions allocate nothing
functions cannot fail
functions are deterministic
```

Tests:

```text
every known status returns expected name
unknown status returns fallback
ok/error predicates behave correctly
```

## 7. Error Contract

Minimum A0 error struct:

```c
typedef struct {
    yvex_status code;
    char where[96];
    char message[256];
} yvex_error;
```

Minimum A0 functions:

```c
void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message);
int yvex_error_is_set(const yvex_error *err);
```

Optional A0 helper:

```c
void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
```

Rules:

```text
clear sets code to YVEX_OK
clear empties where and message
set handles null err by doing nothing
set handles null where using empty string or "unknown"
set handles null message using empty string
strings are truncated safely
strings are always null-terminated
helpers allocate no heap memory
helpers do not abort
helpers do not print to stderr
```

Tests:

```text
clear works
set works
truncation is safe
null inputs do not crash
yvex_error_is_set returns false for YVEX_OK
yvex_error_is_set returns true for error codes
```

## 8. Logging Contract

A0 logging is minimal. It is a naming and enum surface, not a full logging
subsystem.

Minimum levels:

```c
typedef enum {
    YVEX_LOG_ERROR,
    YVEX_LOG_WARN,
    YVEX_LOG_INFO,
    YVEX_LOG_DEBUG,
    YVEX_LOG_TRACE
} yvex_log_level;
```

Minimum domains:

```c
typedef enum {
    YVEX_LOG_CORE,
    YVEX_LOG_CLI
} yvex_log_domain;
```

A0 exposes only `CORE` and `CLI` log domains. Future domains are documented in
`docs/logging-tracing.md` and are added to public headers only when their
modules exist.

Minimum functions:

```c
const char *yvex_log_level_name(yvex_log_level level);
const char *yvex_log_domain_name(yvex_log_domain domain);
```

A0 does not need:

```text
full logger
timestamps
JSON logs
file logs
trace system
global runtime config
```

Rules:

```text
unknown level/domain returns fallback
functions allocate nothing
functions require no runtime init
```

## 9. CLI Bootstrap Contract

A0 CLI file:

```text
cli/yvex_cli.c
```

A0 must support:

```text
yvex info
yvex --version
yvex --help
yvex help
yvex help info
```

Allowed optional alias:

```text
yvex version
```

Unknown command:

```sh
yvex unknown
```

Required behavior:

```text
print an error to stderr
print brief help or hint
exit 2
```

No-argument behavior:

```sh
yvex
```

Required behavior:

```text
print concise usage
exit 0
```

`yvex info` output must include:

```text
name: YVEX
version: 0.1.0
language: C
interface: CLI-only
status: pre-implementation runtime skeleton
inference: not implemented
gguf: not implemented
cuda: not implemented
server: not implemented
```

Important:

```text
do not claim GGUF support
do not claim CUDA readiness
do not claim server compatibility
do not claim model execution
do not print fake provider support
```

## 10. CLI Exit Code Subset For A0

A0 implements only:

```text
0 success
1 generic internal error
2 invalid command or invalid arguments
```

The full future matrix remains in `docs/cli-runtime.md`.

A0 must not pretend to implement format, backend, server, or provider exit
classes before those modules exist.

## 11. Makefile System Design

A0 Makefile must produce either this layout:

```text
build/lib/libyvex.a
build/bin/yvex
build/tests/test_status
build/tests/test_error
```

or a similarly documented build layout.

Required targets:

```text
make info
make lib
make cli
make test
make check
make clean
```

Target semantics:

```text
make info
  prints repository/project status
  requires no compilation

make lib
  compiles core objects
  archives libyvex.a

make cli
  depends on lib
  builds yvex

make test
  depends on lib
  builds and runs tests

make check
  runs docs/guardrail checks
  builds lib
  builds CLI
  runs tests

make clean
  removes build artifacts
```

Compiler:

```text
default: cc
override: CC=...
```

Baseline flags:

```text
-std=c11
-Wall
-Wextra
-Iinclude
```

`-Werror` policy:

```text
use -Werror only if the current compiler/environment is controlled enough
otherwise use strict warnings without blocking portability prematurely
the A0 report must state the chosen policy
```

A0 build must not depend on:

```text
CUDA
model files
network
YAI checkout
external package managers
```

## 12. Test Harness Design

A0 tests are simple C binaries, not a framework-heavy subsystem.

Test binaries:

```text
tests/test_status.c
tests/test_error.c
optional tests/test_version.c
optional tests/test_log.c
```

Each test:

```text
returns 0 on success
returns non-zero on failure
prints concise failure message to stderr
uses no external dependency
runs from make test
```

Test helper options:

```text
simple local macros in each test file
or tests/test.h if A0 chooses to create it
```

Do not create an elaborate test framework in A0.

## 13. Build Artifact Policy

Build output should go under:

```text
build/
```

Recommended layout:

```text
build/obj/
build/lib/
build/bin/
build/tests/
```

Generated files must not be committed.

`.gitignore` must include build outputs if not already covered:

```text
build/
*.o
*.a
/yvex
```

A0 may update `.gitignore` if needed.

## 14. Public API Exposure Discipline

A0 public headers must only declare implemented functions.

Forbidden pattern:

```c
typedef struct yvex_model yvex_model;
int yvex_model_open(...);
```

unless `yvex_model_open` exists and is tested.

Allowed future mention:

```text
docs may mention future APIs
headers must not expose unimplemented APIs
```

Reason:

```text
headers are contracts
A0 must not create fake API surface
```

## 15. Future Module Attachment Map

B0 runtime filesystem attaches after:

```text
status
error
log
version
```

B0 may create:

```text
include/yvex/fs.h
src/fs/
tests/test_fs.c
```

C0 artifact/GGUF attaches after:

```text
fs/range helpers
artifact file handling
```

C0 may create:

```text
include/yvex/artifact.h
include/yvex/gguf.h
src/artifact/
src/formats/
fixtures/gguf/
tests/test_artifact.c
tests/test_gguf_header.c
```

D0 tensor/model attaches after:

```text
GGUF metadata and tensor directory
```

D0 may create:

```text
include/yvex/dtype.h
include/yvex/tensor.h
include/yvex/model.h
src/model/
tests/test_dtype.c
tests/test_tensor_table.c
```

E0 tokenizer attaches after:

```text
model descriptor
metadata access
```

F0 graph/planner attaches after:

```text
tensor roles
model profile
```

G0 CPU backend attaches after:

```text
graph op definitions
memory planner skeleton
```

H0 session runtime attaches after:

```text
backend ABI
sampler
KV skeleton
```

I0 CLI chat attaches after:

```text
session event stream
token output path
```

L0 CUDA attaches after:

```text
backend ABI
CPU reference parity path
```

K0 server/provider attaches after:

```text
session runtime
streaming events
receipt model
```

## 16. Runtime Initialization Policy

A0 should avoid global runtime initialization.

No `yvex_init()` is required in A0 unless there is a real need.

Functions in version/status/error/log-name helpers must be usable without init.

Future runtime init may appear when:

```text
config loading exists
filesystem runtime exists
logging sinks exist
backend registry exists
```

If a future init is introduced, it must be explicit and tested.

## 17. Memory And Allocation Policy For A0

A0 core helpers should avoid heap allocation.

Allowed:

```text
stack buffers
fixed error buffers
static constant strings
```

Avoid:

```text
malloc in version/status/log helpers
hidden global dynamic state
heap allocation for CLI help unless necessary
```

If CLI uses dynamic memory, it must be obvious and freed.

## 18. Thread-Safety Policy For A0

A0 core helpers are reentrant if they:

```text
read static constants
write only caller-provided error structs
do not use mutable global state
```

A0 does not claim full thread-safety for the future runtime.

Documented A0 thread-safety:

```text
version/status name helpers are safe
error helpers are safe per caller-owned object
full logging sink is not implemented yet
```

## 19. Documentation Updates Required By P0.8

P0.8 must update `docs/README.md` to include:

```text
runtime-system-design.md
```

P0.8 must update `docs/roadmap.md` to preserve:

```text
P0.8 Runtime / System Design
A0 Code-first C skeleton
```

If `docs/spine.md` has an immediate next milestone, P0.8 must make it name:

```text
P0.8 - Runtime / System Design
```

After P0.8 completion, the spine and roadmap should point to:

```text
A0 - Code-first C skeleton
```

Milestone numbers must not be changed casually.

## 20. Validation For P0.8

Required commands:

```sh
make info
make check
git diff --check
```

ASCII check for changed docs:

```sh
LC_ALL=C grep -n '[^ -~]' docs/runtime-system-design.md docs/README.md docs/roadmap.md docs/spine.md
```

If grep returns nothing, ASCII-clean passes.

No runtime code check:

```sh
for d in include src cli backends tests; do
  if [ -d "$d" ]; then
    find "$d" -type f
  fi
done | sort
```

Expected result:

```text
no runtime .c or .h implementation files introduced by P0.8
```

No forbidden terminal UI paths:

```sh
find . -path './.git' -prune -o \( -path './tui' -o -path './src/tui' -o -path './include/yvex/tui.h' \) -print
```

Legacy naming scan. Use bracketed patterns so the command does not match its
own documentation:

```sh
grep -RIn \
  -e "N[E]T\\.SPINE" \
  -e "N[E]T moves streams" \
  -e "C[L]ORI executes neural computation" \
  -e "libc[l]ori" \
  -e "c[l]orid" \
  -e "include/c[l]ori" \
  -e "~/\\.config/c[l]ori" \
  -e "github.com/yailabs/c[l]ori" \
  -e "yailabs/c[l]ori" \
  --exclude-dir=.git .
```

Allowed result:

```text
no active legacy naming
```

## 21. Manual Proof Report

P0.8 final report must include:

```text
files created
files modified
confirmation no runtime code added
confirmation no headers added
confirmation no CLI binary added
make check result
git diff --check result
no forbidden terminal UI path result
old naming scan result
next milestone: A0, not another invented P0 readiness pass
```

## 22. Completion Criteria

P0.8 is complete when:

```text
docs/runtime-system-design.md exists
it fully defines A0 runtime system structure
docs index links it
roadmap keeps P0.8 as Runtime / System Design
no runtime code exists yet
validation passes
A0 can be executed from this design without ambiguity
```
