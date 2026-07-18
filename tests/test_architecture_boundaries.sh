#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

python3 - <<'PY'
from collections import Counter
from pathlib import Path
import re
import subprocess

production = sorted(
    path for root in (Path("src"), Path("include"))
    for path in root.rglob("*")
    if path.suffix in {".c", ".h", ".cu"}
)
errors = []

for path in production:
    text = path.read_text(errors="ignore")
    includes = re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]', text, re.MULTILINE)
    duplicates = [name for name, count in Counter(includes).items() if count > 1]
    if duplicates:
        errors.append(f"duplicate include in {path}: {duplicates}")
    private_prefixed = re.findall(
        r'^\s*static\b[^\n]*\b(yvex_[A-Za-z0-9_]+)\s*\(',
        text,
        re.MULTILINE,
    )
    if private_prefixed:
        errors.append(
            f"private static functions retain public prefix in {path}: "
            f"{sorted(set(private_prefixed))}"
        )
    source = str(path)
    if source.startswith("src/core/") and any(name.startswith("src/cli/") for name in includes):
        errors.append(f"core depends on CLI: {path}")
    if source.startswith(("src/model/", "src/graph/", "src/runtime/", "src/generation/")) and any(
        name.startswith("src/cli/") for name in includes
    ):
        errors.append(f"domain depends on CLI: {path}")
    if source.startswith("src/graph/") and "/families/" not in source and any(
        name.startswith("src/model/families/") for name in includes
    ):
        errors.append(f"generic graph depends on family implementation: {path}")
    if source == "src/graph/numeric.c" and any("families" in name for name in includes):
        errors.append("generic numeric code depends on a family")
    if source.startswith("src/backend/") and any(name.startswith("src/model/") for name in includes):
        errors.append(f"backend reconstructs model ownership: {path}")
    if source.startswith("src/") and any(name.startswith("tests/") for name in includes):
        errors.append(f"production includes test code: {path}")

planning_text = "\n".join(
    path.read_text(errors="ignore") for path in Path("src/model/compilation").glob("*.c")
)
if re.search(r"\b(pread|read|open|fopen|mmap)\s*\(", planning_text):
    errors.append("model compilation reads payload bytes")

include_roots = [
    Path("."), Path("include"), Path("src/core"), Path("src/cli"),
    Path("src/cli/input"), Path("src/cli/io"), Path("src/cli/model_artifacts"),
    Path("src/cli/render"), Path("src/source"), Path("src/io"),
    Path("src/backend"), Path("src/backend/cuda"), Path("src/runtime"),
    Path("src/server"), Path("src/gguf"), Path("src/generation"),
    Path("src/graph"), Path("src/model/artifacts"),
    Path("src/model/compilation"), Path("src/model/target"),
]
root = Path.cwd().resolve()
nodes = {str(path): set() for path in production}
for path in production:
    for name in re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]', path.read_text(errors="ignore"), re.MULTILINE):
        for candidate in [path.parent / name] + [base / name for base in include_roots]:
            if not candidate.exists():
                continue
            try:
                target = str(candidate.resolve().relative_to(root))
            except ValueError:
                continue
            if target in nodes:
                nodes[str(path)].add(target)
            break

index = 0
indices = {}
low = {}
stack = []
on_stack = set()
cycles = []
def visit(node):
    global index
    indices[node] = low[node] = index
    index += 1
    stack.append(node)
    on_stack.add(node)
    for target in nodes[node]:
        if target not in indices:
            visit(target)
            low[node] = min(low[node], low[target])
        elif target in on_stack:
            low[node] = min(low[node], indices[target])
    if low[node] == indices[node]:
        component = []
        while True:
            target = stack.pop()
            on_stack.remove(target)
            component.append(target)
            if target == node:
                break
        if len(component) > 1:
            cycles.append(sorted(component))
for node in nodes:
    if node not in indices:
        visit(node)
if cycles:
    errors.append(f"include cycles: {cycles}")

library = Path("build/lib/libyvex.a")
if not library.is_file():
    errors.append("symbol audit requires build/lib/libyvex.a")
else:
    output = subprocess.check_output(
        ["nm", "-g", "--defined-only", str(library)], text=True
    )
    defined = [
        fields[2] for line in output.splitlines()
        if len(fields := line.split()) >= 3
        and re.fullmatch(r"[A-ZTRDB]", fields[1])
    ]
    foreign = sorted(name for name in set(defined) if not name.startswith("yvex_"))
    if foreign:
        errors.append(f"globally visible symbols lack yvex_ namespace: {foreign}")
    symbols = [name for name in defined if name.startswith("yvex_")]
    duplicates = [name for name, count in Counter(symbols).items() if count > 1]
    if duplicates:
        errors.append(f"duplicate exported symbols: {duplicates}")
    if len(set(symbols)) > 969:
        errors.append(f"exported symbol count regressed: {len(set(symbols))} > 969")
    family_phase = [name for name in symbols if name.startswith("yvex_deepseek_")]
    if family_phase:
        errors.append(f"family phase symbols escaped: {family_phase}")
    family_objects = {
        "build/obj/src/model/families/deepseek_v4.o": {
            "yvex_model_register_deepseek_v4"
        },
        "build/obj/src/graph/families/deepseek_v4.o": {
            "yvex_graph_lower_deepseek_v4"
        },
        "build/obj/src/backend/cuda/families/deepseek_v4.o": {
            "yvex_backend_attention_execute"
        },
    }
    for object_path, expected in family_objects.items():
        if not Path(object_path).is_file():
            errors.append(f"family symbol audit missing object: {object_path}")
            continue
        object_output = subprocess.check_output(
            ["nm", "-g", "--defined-only", object_path], text=True
        )
        observed = {
            fields[2] for line in object_output.splitlines()
            if len(fields := line.split()) >= 3
            and re.fullmatch(r"[A-ZTRDB]", fields[1])
        }
        if observed != expected:
            errors.append(
                f"family object ABI diverged: {object_path}: "
                f"expected={sorted(expected)} observed={sorted(observed)}"
            )

if errors:
    raise SystemExit("architecture boundaries:\n  " + "\n  ".join(errors))
print(
    "architecture boundaries: ok "
    f"files={len(production)} include_cycles=0 forbidden_edges=0 "
    f"exports={len(set(symbols))} duplicate_exports=0"
)
PY
