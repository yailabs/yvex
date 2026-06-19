# YVEX Validation

This document defines the repository validation posture during the
pre-implementation cutover. The full technical contract lives in `docs/spine.md`.

## Current Phase

P0.3 validates documentation authority and guardrails. It does not compile C
runtime code because A0 has not created runtime code yet.

Current commands:

```sh
make info
make check
```

## Baseline Rule

Baseline validation must not require:

```text
model downloads
CUDA hardware
network access
server startup
YAI checkout
```

## Current Checks

`make check` verifies:

```text
canonical docs exist
docs/spine.md declares YVEX Implementation Spine
docs/spine.md declares CLI-only policy
README.md uses YVEX public identity
legacy NET/CLORI authority phrases are absent from current public docs
forbidden TUI implementation paths are absent
forbidden terminal UI dependencies are absent from current public docs/build files
README.md does not claim implemented inference/server/CUDA support
```

Legacy CLORI/NET scaffold files still exist during the cutover window. They are
not authoritative and will be handled by a dedicated archive pass.

## Future Code-First Checks

A0 and later must add:

```text
C compilation
static library build
CLI build
unit tests
source hygiene checks
range-check tests
error/status tests
no fake support claim scan
no-TUI guard
```

## Future Fixture Checks

Parser and runtime work must add:

```text
golden fixtures
malformed GGUF fixtures
tokenizer fixtures
graph dump fixtures
memory-plan JSON fixtures
sampler deterministic fixtures
CPU/CUDA parity fixtures
```

## Manual Proof

Every delivery box reports:

```text
commands run
validation result
files changed
non-goals preserved
remaining deferred work
```
