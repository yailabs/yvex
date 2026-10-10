<!-- docs:metadata
title: Build and Review Documentation
id: yvex.guides.documentation-build
document: guide
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Build and Review Documentation

**Publish the same Markdown owners to GitHub, a local reader and PDF.**

[Up](README.md)

## Prepare the lightweight toolchain

```sh
make docs-setup
make docs-check
```

Pinned Python Markdown and PyYAML live in ignored `build/docs-venv`. No browser,
model, CUDA or GPU setup is needed for document/link/metadata checks. The existing
structural QA lane also checks source ownership, ABI and executable surfaces.

## Regenerate checked projections

```sh
make docs-diagrams
make docs-benchmarks
make docs-check
```

Diagram JSON owns typed nodes, relationships, labels and static layout. The
renderer embeds the same transparent SVG in Markdown, GitHub and offline
HTML/PDF; Mermaid is an optional graph export. Run `make docs-diagrams` after
changing the source, not after editing a generated block. Follow the
[visual grammar](../reference/DOCUMENTATION-PROTOCOL.md#visual-and-editorial-grammar).

Benchmark JSON owns observed values. `make docs-benchmarks` also refreshes the
bounded benchmark slots in the root README and benchmark landing. The selection
in `tools/docs/benchmarks.py` names exact receipts, never copied values or a
fastest-run heuristic. Incompatible shared configuration refuses publication;
unknown latency remains NOT MEASURED. Update the selected IDs deliberately when
a new publication checkpoint earns evidence. Runtime benchmark schema v5 remains
a separate producer contract.

## Read HTML

```sh
make docs-site
python3 -m http.server 8090 --bind 127.0.0.1 --directory build/docs
```

Open the printed local server address. The reader provides area navigation,
search, figure zoom, large-table filtering and light/dark themes. Static content
still works without JavaScript; search needs HTTP rather than a file URL.

## Produce PDF

```sh
make docs-pdf
# Or select an already installed Chromium executable:
make docs-pdf DOCS_BROWSER=/path/to/chromium
```

The output is `build/docs/yvex-technical-dossier.pdf`; `book.html` is the same
printable projection. The book opens optional technical depth before printing.
An unavailable browser leaves HTML intact and reports the missing PDF tool.
Do not install a heavy UI stack merely to claim visual qualification.

## Review both reading surfaces

Inspect native repository Markdown and the enhanced reader at narrow, tablet
and desktop widths, in light/dark conditions. Check first viewport, middle/end,
table overflow, diagram labels, keyboard focus and reduced motion. GitHub's
Markdown API `markdown` mode can qualify repository-file markup locally; an
API preview is not an inspection of the final pushed blob page.

Document the actual renderer and limitations. A successful render or docs test
qualifies presentation infrastructure, not a model/runtime capability.
