<!-- docs:metadata
title: 0007 — External terminal editor ownership
id: yvex.decisions.0007-external-terminal-editor
document: reference
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0007 — External terminal editor ownership

**Structural selection and its conceptual lineage.**

[Up](README.md)

Date: 2026-09-05

## Context

The chat protocol client previously implemented byte input, raw mode, cursor
movement, history navigation, paste framing and redraw alongside product
commands, sessions and generation. REPLAI now supplies that terminal substrate
through an independently qualified C ABI, including the existing line-oriented
presentation grammar. Keeping two live editors would leave ownership ambiguous.

## Decision

`src/cli/io/client.c` consumes REPLAI C ABI 1 through its installed header. The
exact revision, Git tree and downloaded archive checksum are owned by
`config/replai.json`: active revision `93d62f6d34cfb933a1f59407ade027152e1ef2ba`.
Base C ABI 1 remains unchanged; the separately queried presentation extension 1
adds bounded semantic documents, composed prompts, candidate sets, driven events
and an exclusive quiet-output scope. It exports no Rust layouts or YVEX meaning.
The original cutover qualified `df5538c718b8d068432032e7fb116fb8bfab158e`;
that historical identity remains unchanged in its evidence.
No floating branch, vendored header, committed native artifact or runtime
fallback editor exists. The adjacent REPLAI repository is not required.

The product statically links `libreplai_c.a`. System link flags come from the
staged `replai.pc`. This preserves the single executable package and introduces
no runtime loader requirement for a REPLAI shared library. The private prefix
contains the producer's original header, native artifacts, license and a
checksum receipt. Product packages retain its license and build receipt.

`make client` builds the pinned external producer when needed. The build requires
current stable Rust/Cargo, Python 3, pkg-config and the existing native compiler.
The producer downloads the immutable archive over HTTPS, checks its SHA-256,
builds with `cargo build --locked --release -p replai-c`, and invokes REPLAI's
own staging command. Source and Cargo target files live in a temporary external
cache workspace, not in Git. A failed preparation never publishes a partial
prefix. Repeated builds verify installed checksums and reuse the installation.

`REPLAI_PREFIX` can select an empty installation location. `REPLAI_SOURCE` can
select a clean adjacent checkout **at the same exact pin**; its source is not
modified and Cargo output remains external. Incompatible source revisions,
header ABI, receipts, missing artifacts or changed artifact bytes fail the
build. Changing the pin requires a fresh prefix and consumer requalification.

The adapter owns one opaque handle per chat lifetime. Each input opens with a
semantic composed host label/state prompt and closes before product dispatch.
Submission copies exact UTF-8 into a C-owned command buffer. YVEX admits history,
supplies registry-authored candidates and bounded context from resident session
names or attachment paths. REPLAI owns menus, label/annotation layout, navigation
and insertion. Enter accepts a selected candidate without submitting the line.
Snapshot tickets refuse stale draft results; completion never admits an attachment.

The POSIX host waits on REPLAI's borrowed input FD, its own wake pipe and the
producer's next decoder deadline. SIGWINCH only wakes that ordinary control flow;
SIGINT enters the existing counted cancellation policy. Both prior handlers are
restored before the scope closes. REPLAI owns no signal handler or host thread.
Visual Esc dismissal is immediate while the original fragmented-sequence deadline
remains intact. No timeout was shortened to mask a decoder ambiguity.

Generation starts after editor restoration. REPLAI's separately admitted output
scope owns temporary echo suppression, replaceable progress and captured-terminal
restoration. YVEX owns cancellation, queued-input discard, exact channels,
transactions, session generation and reconnect. There is no concurrent editable
prompt during generation, alternate screen or terminal background painting.

Ordinary CLI renderers construct semantic records or comparative tables, using the
same pure REPLAI document renderer without an editor lifetime. JSON serialization
remains separate. Palette and Unicode cell measurement have one producer owner;
YVEX retains bounded stream framing, trusted structural projection and control-byte
escaping. Complex progressive grapheme wrapping is not a universal terminal claim.

## Consequences and evidence

The old live `repl_read_line`, byte insertion/deletion, escape reader, scalar
column count, redraw and history-navigation implementation is removed. Manual table padding/elision, scalar/wcwidth cell measurement, the duplicate
palette and generation termios implementation are also removed. Domain formatting
and semantic helpers with historical `repl_` names are not a second editor.

`tests/repl_pty.sh` retains its existing attachments, streaming, cancellation,
reconnect and linear-surface coverage. Its production-process extension observes
exact protocol input after Unicode/grapheme edits, history return, multiline CRLF
paste and slash completion. It checks prompt bytes, color-disable rules, resize,
Ctrl-L, editing interrupts and exact captured termios. TTY descriptors are 5
while editing, 5 in the separately admitted quiet-output scope, and 5 after reopening; repeated turns retain
the same count. Optional `YVEX_REPL_MEMCHECK=valgrind make test-repl` runs the real
chat process under a memory checker and fails on invalid accesses/definite leaks.

The existing tiny vertical also drives `yvex chat` against `yvex serve` and the
real compiled CPU decoder: input `a` produces `okokok`, advances session position
to 5, returns to the prompt, resets through a product command, and repeats. This
is bounded executable composition evidence, not model quality or a large-model
performance result. No weights are downloaded for terminal qualification.

## Alternatives considered

Runtime shared linkage was unnecessary for the current executable package.
Vendoring, copying the binding, a local editor fallback and a toy acceptance
consumer would not prove external ownership. Extending REPLAI with product
semantics was rejected; base ABI 1 remains compatible. The 2026-09-30 implementation adds an independently
qualified optional extension rather than reinterpreting that ABI.
