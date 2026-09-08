#!/usr/bin/env python3
"""Validate the small current YVEX documentation surface."""

from __future__ import annotations

import re
import argparse
import hashlib
import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    "README.md",
    "ROADMAP.md",
    "CONTRIBUTING.md",
    "NOTICE.md",
    "SECURITY.md",
    "SUPPORT.md",
    "CHANGELOG.md",
    "AGENTS.md",
    "docs/README.md",
    "docs/architecture/system.md",
    "docs/architecture/compilation.md",
    "docs/architecture/runtime.md",
    "docs/architecture/commands.md",
    "docs/contracts/artifacts.md",
    "docs/contracts/c-api.md",
    "docs/contracts/events-telemetry.md",
    "docs/contracts/local-protocol.md",
    "docs/contracts/runtime.md",
    "docs/development/agentic-engineering.md",
    "docs/development/qa.md",
    "docs/development/source-ownership.md",
    "docs/model-families/integration.md",
    "docs/model-families/deepseek-v4-flash.md",
    "docs/model-families/minimax-h3.md",
    "docs/model-families/mamba2.md",
    "docs/openai-compatibility.md",
    "docs/operator-runbook.md",
    "docs/releases/doctrine.md",
    "docs/releases/v0.1.md",
}
RETIRED_PATHS = {
    "STATUS.md",
    "FEATURES.md",
    "CAPABILITY_MATRIX.md",
    "PLAN.md",
    "MILESTONES.md",
    "PROJECT.md",
    "MODEL_ARTIFACTS.md",
    ".agents/skills/engineering-worklog",
    "config/documentation_owners.tsv",
    "config/frozen_documents.tsv",
    "docs/audits",
    "docs/doctrine",
    "docs/migrations",
    "docs/milestones",
    "docs/operations",
    "docs/worklog",
    "docs/archive",
    "docs/ledger",
    "docs/agentic",
    "docs/roadmap",
    "docs/development/documentation-policy.md",
}

ROADMAP_SECTIONS = (
    "At a Glance / Current Snapshot", "System Maturity", "Strategic Programs",
    "Architecture Spectrum", "Current Execution Sequence",
    "General Substrate Progression", "v0.1 Release Path",
    "Explicit Nonclaims and Deferred Scope", "Progression and Promotion Discipline",
)
MATURITY_STATES = {
    "🟢 ESTABLISHED": "Generic owner and claimed boundary implemented and qualified at the stated scope.",
    "🟡 PARTIAL": "Real foundation; genericity, breadth, portability, performance or evidence incomplete.",
    "🔴 OPEN": "Generic capability absent or not yet claimable.",
    "⚪ LATER": "Intentionally outside the current maturity horizon.",
}
TEMPORAL_STATES = {"ACTIVE", "NEXT", "PARTIAL", "BLOCKED", "NOT MEASURED", "COMPLETE", "DEFERRED"}
PROGRAMS = set("RCPSGDOMXQF")
COUNTS_START = "<!-- maturity-counts:start -->"
COUNTS_END = "<!-- maturity-counts:end -->"
MATURITY_HEADER = ["Capability", "State", "Current YVEX truth",
                   "Boundary required for promotion", "Program", "Evidence / owner"]


def table_rows(text: str) -> list[list[str]]:
    """The roadmap uses ordinary pipe tables, without multiline/escaped cells."""
    rows = []
    for line in text.splitlines():
        if not line.startswith("|"):
            continue
        if not line.endswith("|"):
            raise ValueError("unterminated roadmap table row")
        cells = [re.sub(r"^(`|\*\*?)(.*?)\1$", r"\2", cell.strip())
                 for cell in line[1:-1].split("|")]
        if all(re.fullmatch(r":?-+:?", cell) for cell in cells):
            continue
        rows.append(cells)
    return rows


def count_projection(counts: dict[str, int]) -> str:
    lines = [COUNTS_START,
             "<!-- Generated from System Maturity by tests/documentation_architecture.py. -->",
             "| Maturity state | Meaning | Current count |", "| --- | --- | ---: |"]
    lines.extend(f"| {state} | {meaning} | {counts[state]} |"
                 for state, meaning in MATURITY_STATES.items())
    return "\n".join(lines + [COUNTS_END])


def validate_roadmap(text: str, *, check_counts: bool = True) -> dict:
    """Validate axes/relations, not incidental prose or today's maturity values."""
    def require(condition: bool, message: str) -> None:
        if not condition:
            raise ValueError(message)

    headings = list(re.finditer(r"(?m)^## (.+)$", text))
    require([h.group(1) for h in headings] == list(ROADMAP_SECTIONS),
            "roadmap must have exactly one of each ordered first-class section")
    sections = {h.group(1): text[h.end():headings[i + 1].start() if i + 1 < len(headings) else len(text)]
                for i, h in enumerate(headings)}
    require(not re.search(r"(?im)^#{2,6} .*snapshot.*\d", text), "dated snapshot history in live roadmap")
    require(text.count(COUNTS_START) == text.count(COUNTS_END) == 1,
            "roadmap requires one generated count projection")
    require(COUNTS_START in sections[ROADMAP_SECTIONS[0]] and
            text.index(COUNTS_START) < text.index(COUNTS_END), "counts belong to Current Snapshot")

    def programs(value: str) -> None:
        ids = value.split(" / ")
        require(bool(ids) and len(ids) == len(set(ids)) and set(ids) <= PROGRAMS,
                f"unknown/duplicate program relation: {value}")

    counts = dict.fromkeys(MATURITY_STATES, 0)
    capabilities = set()
    headers = 0
    for row in table_rows(sections["System Maturity"]):
        if row == MATURITY_HEADER:
            headers += 1
            continue
        if len(row) != 6:
            require(not any(cell in MATURITY_STATES for cell in row),
                    "maturity row outside the six-column schema")
            continue  # Explicitly uncoloured target/design-space tables.
        require(all(row), "empty maturity field")
        capability, state, _, _, program, owner = row
        require(state in counts, f"invalid maturity state: {state}")
        require(capability not in capabilities, f"duplicate maturity capability: {capability}")
        require(bool(re.search(r"\[[^]]+\](?:\([^)]*\)|\[[^]]+\])", owner)),
                f"missing canonical owner link: {capability}")
        programs(program)
        capabilities.add(capability)
        counts[state] += 1
    require(headers > 0 and bool(capabilities), "empty System Maturity")
    projection = count_projection(counts)
    start, end = text.index(COUNTS_START), text.index(COUNTS_END) + len(COUNTS_END)
    require(not check_counts or text[start:end] == projection,
            "stale maturity counts: run --update-roadmap-counts")

    program_rows = table_rows(sections["Strategic Programs"])[1:]
    require(len(program_rows) == len(PROGRAMS), "strategic program count/duplicates")
    require({r[0] for r in program_rows} == PROGRAMS, "strategic program identity mismatch")
    for row in program_rows:
        require(len(row) == 5 and all(row) and row[4] in MATURITY_STATES, "invalid program row")
        if row[4] != "⚪ LATER":
            require(bool(re.search(rf"(?m)^### {row[0]} — ", sections["Strategic Programs"])),
                    f"missing program detail card: {row[0]}")

    for row in table_rows(sections["General Substrate Progression"]):
        if len(row) == 4 and row[0] != "Horizon":
            programs(row[2])
        elif len(row) == 5 and row[0] != "Active / near boundary":
            programs(row[1])

    spectrum = []
    for section, body in sections.items():
        for row in table_rows(body):
            require(not re.fullmatch(r"H\d+", row[0]), "private H classifications in public roadmap")
            if re.fullmatch(r"A\d+", row[0]):
                require(section == "Architecture Spectrum", "spectrum rows outside their owner")
                require(len(row) == 5 and all(row), "invalid spectrum row")
                require(row[4].split(" / ")[0] in {"PLANNED", "PARTIAL", "BLOCKED", "COMPLETE", "DONE", "DEFERRED"},
                        "invalid spectrum evidence state")
                spectrum.append(row[0])
    require(spectrum == [f"A{i:02}" for i in range(1, 11)], "spectrum must preserve ordered A01-A10")

    sequence = table_rows(sections["Current Execution Sequence"])[1:]
    require(bool(sequence), "empty execution sequence")
    ids, active = set(), []
    for number, row in enumerate(sequence, 1):
        require(len(row) == 7 and all(row), "invalid sequence schema")
        require(row[0] == str(number), "sequence order is not consecutive")
        require(row[1] not in ids and bool(re.fullmatch(r"[A-Z0-9_.]+", row[1])), "duplicate/invalid boundary ID")
        require(row[2] in TEMPORAL_STATES, "invalid temporal state")
        programs(row[3])
        ids.add(row[1])
        if row[2] == "ACTIVE":
            active.append(row[1])
    next_ids = re.findall(r"(?m)^Active Next:\s*(\S+)\s*$", text)
    require(len(active) == 1 and next_ids == active, "Active Next and execution sequence disagree")
    snapshot = dict(row for row in table_rows(sections[ROADMAP_SECTIONS[0]]) if len(row) == 2)
    require(re.findall(r"`([^`]+)`", snapshot.get("Active engineering boundary", "")) == active,
            "snapshot and active execution boundary disagree")

    release_rows = table_rows(sections["v0.1 Release Path"])[1:]
    require(all(len(r) == 5 and all(r) and r[1] in TEMPORAL_STATES for r in release_rows), "invalid release gate")
    require(len({r[0] for r in release_rows}) == len(release_rows), "duplicate release gate")
    gates = {r[0]: r[1] for r in release_rows}
    for flag, gate in (
        ("model_behavior_evaluation_ready", "Model behavior evaluation"),
        ("full_model_release_benchmark_ready", "Full-model benchmark"),
        ("release_qualification_ready", "Release qualification"),
    ):
        require(gate in gates, f"missing release gate: {gate}")
        require(re.findall(rf"(?m)^{flag}=([01])$", text) == [str(int(gates[gate] == "COMPLETE"))],
                f"release flag and gate disagree: {flag}")
    return {"counts": counts, "rows": len(capabilities), "active": active[0],
            "sequence": len(sequence), "projection": projection}


def check_roadmap() -> None:
    text = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    try:
        result = validate_roadmap(text)
        # Mutations exercise relationships without freezing current prose/statuses.
        first_row = next(line for line in text.splitlines() if "| 🟢 ESTABLISHED |" in line and line.count("|") == 7)
        first_sequence = next(line for line in text.splitlines() if line.startswith("| 1 | `"))
        mutations = {
            "duplicate section": text + "\n## System Maturity\n",
            "missing section": text.replace("## Strategic Programs", "## Programs", 1),
            "stale count": text.replace("| Current count |", "| Stale count |", 1),
            "invalid maturity": text.replace(first_row, first_row.replace("🟢 ESTABLISHED", "GREEN"), 1),
            "duplicate capability": text.replace(first_row, first_row + "\n" + first_row, 1),
            "missing owner": text.replace(first_row, first_row.rsplit("|", 2)[0] + "| no owner |", 1),
            "unknown program": text.replace(first_row, first_row.rsplit("|", 3)[0] + "| Z | [owner](README.md) |", 1),
            "stale active": text.replace("Active Next: " + result["active"], "Active Next: UNKNOWN.0", 1),
            "snapshot drift": text.replace("`" + result["active"] + "`", "`UNKNOWN.0`", 1),
            "two active rows": text.replace(first_sequence, re.sub(
                r"(\| 1 \| `[^`]+` \| )[^|]+", r"\1ACTIVE ", first_sequence), 1)
                if "| ACTIVE |" not in first_sequence else text.replace("| ACTIVE |", "| COMPLETE |", 1),
            "private classifications": text + "\n| H01 | private |\n",
            "spectrum elsewhere": text + "\n| A01 | elsewhere |\n",
            "missing program": text.replace("| F | Scale-out", "| Z | Scale-out", 1),
            "false release": text.replace("release_qualification_ready=0", "release_qualification_ready=1", 1)
                if "release_qualification_ready=0" in text else text.replace("release_qualification_ready=1", "release_qualification_ready=0", 1),
        }
        for label, bad in mutations.items():
            try:
                validate_roadmap(bad, check_counts=False if label != "stale count" else True)
            except ValueError:
                continue
            raise ValueError(f"roadmap guard accepted {label}")
        require_count = text.replace(first_row, first_row.replace("🟢 ESTABLISHED", "🟡 PARTIAL"), 1)
        moved = validate_roadmap(require_count, check_counts=False)
        start = require_count.index(COUNTS_START)
        end = require_count.index(COUNTS_END) + len(COUNTS_END)
        regenerated = require_count[:start] + moved["projection"] + require_count[end:]
        if validate_roadmap(regenerated)["counts"] == result["counts"]:
            raise ValueError("count regeneration ignored changed maturity")
        print(f"roadmap: ok (maturity={result['rows']} active={result['active']} negative={len(mutations)})")
    except ValueError as exc:
        fail(str(exc))


def fail(message: str) -> None:
    print(f"documentation architecture: {message}", file=sys.stderr)
    raise SystemExit(1)


def markdown_paths() -> set[str]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", "*.md"],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    return {
        line
        for line in result.stdout.splitlines()
        if line and (ROOT / line).is_file() and not line.startswith("build/")
    }


def github_anchors(path: Path) -> set[str]:
    anchors: set[str] = set()
    counts: dict[str, int] = {}
    in_fence = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        if raw.lstrip().startswith(("~~~", "```")):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", raw)
        if not match:
            continue
        heading = re.sub(r"!?\[([^]]*)\]\([^)]+\)", r"\1", match.group(1))
        heading = re.sub(r"<[^>]+>", "", heading)
        slug = heading.replace("`", "").replace("*", "").strip().lower()
        slug = re.sub(r"[^\w\- ]", "", slug, flags=re.UNICODE)
        slug = re.sub(r"\s+", "-", slug)
        ordinal = counts.get(slug, 0)
        counts[slug] = ordinal + 1
        anchors.add(slug if ordinal == 0 else f"{slug}-{ordinal}")
    return anchors


def link_targets(text: str) -> list[str]:
    """Current docs use inline/reference Markdown and HTML image/link targets."""
    text = re.sub(r"(?ms)^\s*(`{3,}|~{3,})[^\n]*\n.*?^\s*\1\s*$", "", text)
    pattern = re.compile(r"!?\[[^]]*\]\(([^)\s]+)(?:\s+['\"][^'\"]*['\"])?\)")
    references = re.findall(r"(?m)^\s*\[[^]]+\]:\s*(\S+)", text)
    definitions = {key.casefold() for key in re.findall(r"(?m)^\s*\[([^]]+)\]:", text)}
    for label, key in re.findall(r"!?\[([^]\n]+)\]\[([^]\n]*)\]", text):
        if (key or label).casefold() not in definitions:
            fail(f"undefined Markdown reference: [{label}][{key}]")
    html = re.findall(r"\b(?:src|href)=[\"']([^\"']+)[\"']", text)
    return pattern.findall(text) + references + html


def check_links(paths: set[str]) -> set[Path]:
    anchor_cache: dict[Path, set[str]] = {}
    destinations: set[Path] = set()
    for relative in sorted(paths):
        source = ROOT / relative
        for raw_target in link_targets(source.read_text(encoding="utf-8")):
            target = raw_target.strip("<>")
            if target.startswith(("http://", "https://", "mailto:", "data:")):
                continue
            target, _, fragment = target.partition("#")
            destination = source if not target else (source.parent / unquote(target)).resolve()
            try:
                destination.relative_to(ROOT)
            except ValueError:
                fail(f"{relative} links outside repository: {raw_target}")
            if destination.is_dir():
                destination = destination / "README.md"
            if not destination.exists():
                fail(f"{relative} has unresolved link: {raw_target}")
            destinations.add(destination)
            if fragment and destination.suffix == ".md":
                anchors = anchor_cache.setdefault(destination, github_anchors(destination))
                if fragment not in anchors:
                    fail(f"{relative} has unresolved anchor: {raw_target}")
    return destinations


def check_assets(destinations: set[Path]) -> None:
    """Visual projections need real readers, paired sources, and accessible SVGs."""
    assets = set((ROOT / "docs/diagrams").glob("*"))
    assets.update((ROOT / "docs").glob("*.svg"))
    digests: dict[str, Path] = {}
    for path in sorted(assets):
        if path not in destinations:
            fail(f"unconsumed documentation asset: {path.relative_to(ROOT)}")
        if path.suffix == ".json" and path.with_suffix(".svg") not in assets:
            fail(f"diagram lacks SVG projection: {path.name}")
        if path.suffix != ".svg":
            continue
        if path.parent.name == "diagrams" and path.with_suffix(".json") not in assets:
            fail(f"diagram lacks editable source: {path.name}")
        text = path.read_text(encoding="utf-8")
        for marker in ('<svg ', '<title ', '<desc ', 'role="img"'):
            if marker not in text:
                fail(f"inaccessible SVG {path.name}: absent {marker}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest in digests:
            fail(f"duplicate visual assets: {digests[digest].name}, {path.name}")
        digests[digest] = path


def check_figure_generation() -> None:
    """Exercise the real renderer, plus adversarial layout and stale-output seams."""
    spec = importlib.util.spec_from_file_location("render_diagrams", ROOT / "tools/render_diagrams.py")
    renderer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(renderer)
    numbers = set()
    for path in sorted((ROOT / "docs/diagrams").glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if data["number"] in numbers:
            fail("duplicate canonical figure number")
        numbers.add(data["number"])
        svg = renderer.render(data)
        if svg != renderer.render(copy.deepcopy(data)):
            fail(f"non-deterministic figure: {path.name}")
        ET.fromstring(svg)
        owner = (ROOT / data["owner"]).read_text(encoding="utf-8")
        if path.name not in owner or path.with_suffix(".svg").name not in owner:
            fail(f"figure has no consuming owner: {path.name}")
        for mutation in ("overflow", "overlap", "diagonal", "crossing", "authority", "class", "unknown"):
            bad = copy.deepcopy(data)
            if mutation == "overflow":
                bad["nodes"][0]["lines"][0] = "overflow " * 100
            elif mutation == "overlap":
                bad["nodes"][1]["box"] = list(bad["nodes"][0]["box"])
            elif mutation == "diagonal":
                bad["edges"][0]["points"] = [[40, 100], [60, 120]]
            elif mutation == "crossing":
                x, y, w, h = bad["nodes"][0]["box"]
                bad["edges"][0]["points"] = [[x, y+h/2], [x+w, y+h/2]]
            elif mutation == "authority":
                bad["authority"] = ["/outside-repository"]
            elif mutation == "class":
                bad["nodes"][0]["kind"] = "invented"
            else:
                bad["unsupported"] = True
            try:
                renderer.render(bad)
            except ValueError:
                continue
            fail(f"figure renderer accepted {mutation}: {path.name}")
        with tempfile.TemporaryDirectory(prefix="yvex-figure-check-") as temporary:
            output = Path(temporary) / "fixture.svg"
            output.write_text(svg, encoding="utf-8")
            renderer.check_output(output, svg)
            output.write_text(svg + "stale", encoding="utf-8")
            try:
                renderer.check_output(output, svg)
            except ValueError:
                if output.read_text(encoding="utf-8") != svg + "stale":
                    fail("stale-output check mutated the projection")
            else:
                fail("stale SVG was accepted")
    if len(numbers) != 7:
        fail(f"unexpected canonical figure set: {len(numbers)}")
    result = subprocess.run([sys.executable, str(ROOT / "tools/render_diagrams.py"), "--check"],
                            cwd=ROOT, check=False)
    if result.returncode:
        fail("figure source and SVG are not synchronized")
    for path in (ROOT / "README.md", ROOT / "docs/architecture/system.md"):
        if "```mermaid" in path.read_text(encoding="utf-8"):
            fail(f"duplicate inline architecture figure: {path.name}")


def check_current_truth(paths: set[str]) -> None:
    for required in REQUIRED:
        if required not in paths:
            fail(f"missing current documentation owner: {required}")
    for retired in RETIRED_PATHS:
        if (ROOT / retired).exists():
            fail(f"retired governance surface remains: {retired}")

    active = []
    for relative in sorted(paths):
        text = (ROOT / relative).read_text(encoding="utf-8")
        if "Active Next:" in text:
            active.append(relative)
        if relative in {"README.md", "CONTRIBUTING.md", "SECURITY.md", "SUPPORT.md"}:
            if "`yvexd`" in text:
                fail(f"public guidance names the retired daemon executable: {relative}")
        if (
            "PROJECT.md" in text
            and relative != "ROADMAP.md"
            and not relative.startswith("docs/decisions/")
        ):
            fail(f"current documentation points at retired PROJECT.md: {relative}")
    if active != ["ROADMAP.md"]:
        fail(f"Active Next must exist only in ROADMAP.md: {active}")

    for relative in ("README.md", "docs/README.md"):
        text = (ROOT / relative).read_text(encoding="utf-8")
        if re.search(r"^\|\s*[`*]*[AH]\d{2}[`*]*\s*\|", text, re.MULTILINE):
            fail(f"public entry copies private alignment rows: {relative}")
    method = (ROOT / "docs/development/agentic-engineering.md").read_text(encoding="utf-8")
    for section in ("## Authorities", "## Verify the delivery", "## Decide progression",
                    "## Documentation lifecycle"):
        if section not in method:
            fail(f"engineering method lacks authority: {section}")

    check_roadmap()

    server = (ROOT / "include/yvex/server.h").read_text(encoding="utf-8")
    match = re.search(r"#define YVEX_LOCAL_PROTOCOL_VERSION (\d+)u", server)
    if not match:
        fail("public local protocol version is absent")
    contract = (ROOT / "docs/contracts/local-protocol.md").read_text(encoding="utf-8")
    if f"YVEX_LOCAL_PROTOCOL_VERSION = {match.group(1)}" not in contract:
        fail("local protocol document does not match the public header")

    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    for phrase in (
        "## Why YVEX",
        "## Quick start",
        "## Product boundary",
        "## Documentation",
        "## Current limits",
    ):
        if phrase not in readme:
            fail(f"README lacks current entry section: {phrase}")
    if re.search(r"V010\.|POST010\.|/home/|/Users/|\$HOME/", readme):
        fail("README contains project-control or machine-local detail")
    if re.search(
        r"production-ready|blazing fast|state of the art|enterprise-grade|"
        r"seamless|cutting-edge|revolutionary",
        readme,
        re.IGNORECASE,
    ):
        fail("README contains unsupported marketing language")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--roadmap-only", action="store_true", help="check project-control topology and relations")
    actions.add_argument("--update-roadmap-counts", action="store_true", help="regenerate only the maturity count projection")
    args = parser.parse_args()
    if args.update_roadmap_counts:
        path = ROOT / "ROADMAP.md"
        text = path.read_text(encoding="utf-8")
        try:
            result = validate_roadmap(text, check_counts=False)
        except ValueError as exc:
            fail(str(exc))
        start, end = text.index(COUNTS_START), text.index(COUNTS_END) + len(COUNTS_END)
        updated = text[:start] + result["projection"] + text[end:]
        validate_roadmap(updated)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
        print(f"roadmap counts: synchronized ({result['rows']} rows; {result['counts']})")
        return 0
    if args.roadmap_only:
        check_roadmap()
        return 0
    paths = markdown_paths()
    check_current_truth(paths)
    check_assets(check_links(paths))
    check_figure_generation()
    print(f"documentation architecture: ok (markdown={len(paths)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
