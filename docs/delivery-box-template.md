# YVEX Delivery Box Template

This document extracts the delivery-box standard from `docs/spine.md`. The
spine remains authoritative.

Every implementation or documentation wave uses this shape:

```text
ID:
Title:
Goal:
Context:
Scope:
Non-goals:
Files to create:
Files to modify:
Public APIs:
Internal modules:
CLI commands:
Runtime files:
Tests:
Manual proof:
Failure modes:
Validation:
Report notes:
```

## Example

```text
ID: I0.1
Title: linenoise REPL skeleton
Goal: Create interactive yvex chat input loop without model execution.
Context: CLI runtime must be ready before inference so streaming layout is not retrofitted later.
Scope: cli/command_chat.c, src/chat/chat.c, src/terminal/status_line.c.
Non-goals: no tokenizer, no inference, no server, no TUI.
CLI commands: yvex chat --mock
Tests: test terminal command parser
Manual proof: yvex chat opens prompt, /help works, /quit exits.
Validation: make check
```

## Rules

```text
one wave has one main reason to exist
non-goals are explicit
support claims require commands
validation is mandatory
runtime code waits for A0
docs extraction must not rewrite history silently
```
