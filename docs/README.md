<!-- docs:metadata
title: YVEX Documentation
id: yvex.docs
document: reference
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Documentation

**Run a model. Understand its execution. Follow the evidence.**

[YVEX](../README.md)

## Choose a reading path

| Intent | Start here | Then follow |
| --- | --- | --- |
| Understand YVEX | [Vision](product/VISION.md) | [System architecture](architecture/README.md) → [Status](project-control/STATUS.md) |
| Understand compilation | [Compiler](architecture/compiler-ir.md) | [Representation](architecture/representation-artifacts.md) → [Contracts](contracts/README.md) |
| Understand execution | [Runtime](architecture/runtime-lifecycle.md) | [State](architecture/computational-state.md) → [Backend](architecture/backend-execution.md) |
| Add a model family | [Family integration](model-families/integration.md) | [Compiler](architecture/compiler-ir.md) → [Evaluation](evaluation/README.md) |
| Evaluate performance | [Benchmarks](evaluation/benchmarks/README.md) | Methodology → exact observation → limitations |
| Operate YVEX | [Quick Start](guides/quickstart.md) | [Model lifecycle](guides/model-lifecycle.md) → [Runbook](guides/operator-runbook.md) |
| Develop on macOS / Apple Silicon | [Native build](guides/build.md#macos-native-cpu-build) | [Early Metal backend](architecture/backend-execution.md#apple-silicon-metal-foundation) → [Integrated evidence](evaluation/macos-main-integration.md) |
| Integrate an application | [Contracts](contracts/README.md) | C / local protocol / OpenAI / authenticated network management |
| Work as an agent | [AGENTS](../AGENTS.md) | [Selected Tasks](project-control/TASKS.md) → affected plane → evidence |

## Documentation map

| Understand the system | Use or extend it | Verify its boundaries |
| --- | --- | --- |
| [Product purpose](product/README.md) | [Practical guides](guides/README.md) | [Evaluation and benchmarks](evaluation/README.md) |
| [Architecture and ownership](architecture/README.md) | [Integration contracts](contracts/README.md) | [Status and selected Tasks](project-control/README.md) |
| [Structural decisions](decisions/README.md) | [Model-family semantics](model-families/README.md) | [Release evidence](releases/README.md) |
| [Research horizons](research/README.md) | [Commands and terminology](reference/README.md) | [Documentation rules](reference/DOCUMENTATION-PROTOCOL.md) |

## Authority at a glance

Implementation and exact contracts answer what executes. Status answers what
that evidence supports. Tasks answers what is selected. Research answers what
is proposed and how to falsify it. ADRs explain structural selections.
Generated readers, figures and PDFs are projections of those owners.

## Repository governance

[Contributing](../CONTRIBUTING.md) · [License](../LICENSE) · [Notice](../NOTICE.md) ·
[Security](../SECURITY.md) · [Support](../SUPPORT.md) · [Releases](releases/README.md) ·
[Long-horizon Roadmap](../ROADMAP.md)
