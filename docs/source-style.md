# YVEX Source Style

Date: 2026-06-19
Status: A0.1 source discipline contract
Applies to: core headers, C sources, tests, Makefile, and future implementation waves

Authority:
- `docs/spine.md` remains canonical.
- This document narrows source/file discipline for implementation work.

Validation:
- `make check`
- `make smoke`
- `git diff --check`

## 0. Authority

This document defines how YVEX source-like files are written. It exists because
the first C skeleton must be maintainable, not merely compilable.

Every new implementation file must make its purpose, ownership, non-ownership,
and validation path clear at the top of the file.

## 1. File Header Discipline

Every tracked source-like file starts with a file header.

Applies to:

```text
public .h files
implementation .c files
test .c files
test helper .h files
Makefile
technical markdown docs
shell test scripts
```

The header is not decoration. It states:

```text
purpose
layer
owned symbols or behavior
explicit non-ownership
validation commands
```

## 2. Public Header Style

Template:

```c
/*
 * YVEX - <Short module name>
 *
 * File: include/yvex/<file>.h
 * Layer: public core API
 *
 * Purpose:
 *   <Concrete description of what this header exposes.>
 *
 * Owns:
 *   - <public type/function>
 *
 * Does not own:
 *   - <future/runtime concern not owned here>
 *
 * Used by:
 *   - libyvex core
 *   - yvex CLI
 *   - tests
 *
 * Validation:
 *   - make check
 *   - <specific test binary or command>
 */
#ifndef YVEX_EXAMPLE_H
#define YVEX_EXAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* declarations */

#ifdef __cplusplus
}
#endif

#endif /* YVEX_EXAMPLE_H */
```

Rules:

```text
use include guards
use extern "C" guards
include only what is needed
declare only implemented APIs
do not expose fake future opaque objects
do not expose backend-specific types in generic headers
```

## 3. C Implementation Style

Template:

```c
/*
 * YVEX - <Short module name>
 *
 * File: src/core/<file>.c
 * Layer: core implementation
 *
 * Purpose:
 *   <Concrete description of the implementation in this file.>
 *
 * Implements:
 *   - <function>
 *
 * Invariants:
 *   - <important invariant>
 *
 * Commands:
 *   - make check
 *   - <specific direct binary/test command>
 */
```

Rules:

```text
keep dependencies one-way and local
do not make lower-level core files depend on CLI or future runtime modules
avoid heap allocation in early core helpers
avoid hidden global mutable state
prefer explicit fallback strings for unknown enum values
```

## 4. Test File Style

Template:

```c
/*
 * YVEX - <module> tests
 *
 * File: tests/<file>.c
 * Layer: test
 *
 * Purpose:
 *   <What this test proves.>
 *
 * Covers:
 *   - <function or behavior>
 *
 * Commands:
 *   - make test-core
 *   - build/tests/<test_binary>
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
```

Test rules:

```text
tests return 0 on success
tests print concise failures to stderr
tests use no external test framework in A0.1
tests must cover fallback paths, not just happy paths
```

## 5. Makefile Command Style

The Makefile starts with a build-system header and exposes stable developer
commands.

Required commands:

```text
make info
make lib
make cli
make test-core
make test-cli
make test
make smoke
make check-docs
make check-guardrails
make check
make clean
```

Rules:

```text
make check proves docs, guardrails, library, CLI, unit tests, and CLI smoke
make smoke proves direct CLI bootstrap behavior
build outputs stay under build/
build outputs are not committed
```

## 6. CLI Command Metadata Style

The CLI uses a command table, not a loose string-comparison chain.

Required command metadata:

```c
typedef int (*yvex_cli_handler)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *summary;
    const char *usage;
    const char *description;
    yvex_cli_handler handler;
} yvex_cli_command;
```

Rules:

```text
list only implemented commands
unknown command exits 2
unknown help topic exits 2
future commands remain in docs until implementation exists
```

## 7. Error-Handling Style

Core error helpers use caller-owned fixed-size buffers.

Rules:

```text
null yvex_error pointers are tolerated by clear/set helpers
strings are always null-terminated
truncation is safe
helpers do not allocate heap memory
helpers do not print to stderr
helpers do not abort
```

CLI error style:

```text
human command errors go to stderr
unknown command exits 2
successful command output goes to stdout
```

## 8. Allocation Policy For Early Core Modules

A0.1 core helpers avoid heap allocation.

Allowed:

```text
static constant strings
caller-owned structs
stack-local values
fixed-size public buffers
```

Not allowed in early core helpers:

```text
hidden heap allocation
implicit runtime initialization
background threads
filesystem reads
environment reads
```

## 9. Non-Fake API Exposure Rule

Headers are contracts.

Do not add public declarations for:

```text
artifact loading
GGUF parsing
tokenizer
tensor/model
backend
session
server
provider adapters
```

until the corresponding module exists, compiles, and has tests.

Docs may describe future APIs. Public headers may not pretend they exist.

## 10. Validation Commands

A0.1 validation:

```sh
make clean
make check
make smoke
git diff --check
```

Direct CLI proof:

```sh
./build/bin/yvex
./build/bin/yvex --help
./build/bin/yvex -h
./build/bin/yvex --version
./build/bin/yvex version
./build/bin/yvex info
./build/bin/yvex commands
./build/bin/yvex help info
./build/bin/yvex unknown
```

Guardrail proof:

```sh
test ! -e include/yvex/tui.h
test ! -e include/yvex/gguf.h
test ! -e include/yvex/model.h
test ! -d backends
test ! -d fixtures
```
