# YVEX Operator Runbook

Date: 2026-07-10
Status: runbook index and repository operation boundary

## Purpose

This document routes operators to current executable procedures. It is not a
command catalogue, delivery ledger, capability dashboard, or substitute for
`./yvex help`.

The v0.1.0 product target is DeepSeek-V4-Flash from:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

The future canonical full target is `deepseek4-v4-flash`. It is unsupported.
There is no supported model generation procedure today.

## Runbook Index

| Runbook | Current purpose | Capability boundary |
| --- | --- | --- |
| `runbooks/deepseek.md` | Exact v0.1.0 source/target boundary and safe filesystem checks | no complete artifact or runtime procedure |
| `runbooks/common.md` | Build, validation, documentation guards, artifact hygiene, and operator-local cleanup | validation does not create runtime capability |

Model-family architecture is defined in `model-families.md`. Release gates are
defined in `v010-release-doctrine.md`. Current project state, dependencies, and
Active Next are defined only in `../PROJECT.md`.

## Current Entry

Use the DeepSeek runbook to inspect the exact source boundary. Use the common
runbook for repository validation. Do not substitute selected aliases,
diagnostic commands, fixture output, or report-only surfaces for a full-target
operator run.

Consult `../PROJECT.md` before selecting work. This runbook does not mirror the
current milestone. Until a supported generation procedure is documented here,
the valid operator action is validation and explicit refusal, not DeepSeek
generation.

## Operator-Local State

The following remain outside git:

- model sources and emitted GGUF files;
- local registries and artifact identities;
- reports, logs, pid files, caches, and partial downloads;
- generated backend outputs and build products.

Repository guardrails are listed in `runbooks/common.md` and
`MODEL_ARTIFACTS.md` at the repository root.
