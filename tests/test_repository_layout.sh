#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

python3 - <<'PY'
from pathlib import Path
from collections import Counter
import re
import subprocess

roots = [Path(name) for name in ("src", "include", "tests", "tools", "scripts", "schema", "config") if Path(name).exists()]
source_suffixes = {".c", ".h", ".cu", ".sh", ".py", ".json", ".def", ".tsv"}
owned = sorted(path for root in roots for path in root.rglob("*") if path.is_file())

errors = []
for path in owned:
    if path.suffix not in source_suffixes:
        continue
    basename = path.name
    stem = basename.rsplit(".", 1)[0]
    if basename.startswith("yvex_"):
        errors.append(f"project prefix in filename: {path}")
    if len(basename) > 32:
        errors.append(f"basename exceeds 32 characters: {path}")
    if not re.fullmatch(r"[a-z0-9_]+", stem):
        errors.append(f"filename is not lowercase snake_case: {path}")
    if re.search(r"_(internal|private)\.(c|h|cu|sh|py)$", basename):
        errors.append(f"forbidden phase/private filename: {path}")
    if len(path.parts) > 5:
        errors.append(f"path exceeds five components: {path}")
    directory = path.parent.name
    if directory and directory in stem.split("_"):
        errors.append(f"basename repeats immediate directory: {path}")

production = sorted(
    path for root in (Path("src"), Path("include"))
    for path in root.rglob("*")
    if path.suffix in {".c", ".h", ".cu"}
)
if len(production) > 306:
    errors.append(f"production file count regressed: {len(production)} > 306")

makefile = Path("Makefile").read_text()
if "$(OBJ_DIR)/%.o: %.c" not in makefile or "@mkdir -p $(@D)" not in makefile:
    errors.append("Makefile does not preserve source-relative object paths")
if re.search(r"(?:notdir|basename)\s+\$\([^)]*SRCS", makefile):
    errors.append("Makefile flattens source object names")

make_db = subprocess.run(
    ["make", "-pn"], text=True, stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL, check=False,
).stdout
declared = set()
declared_entries = []
for variable in ("CORE_SRCS", "CLI_SRCS", "CUDA_SRCS", "CUDA_CU_SRCS"):
    match = re.search(rf"^{variable} := (.*)$", make_db, re.MULTILINE)
    if not match:
        errors.append(f"Makefile source variable missing: {variable}")
        continue
    entries = [token for token in match.group(1).split() if token.startswith("src/")]
    declared_entries.extend(entries)
    declared.update(entries)
declared.add("src/daemon/yvexd.c")
duplicates = sorted(entry for entry, count in Counter(declared_entries).items() if count > 1)
if duplicates:
    errors.append(f"duplicate Makefile source entries: {duplicates}")
actual_sources = {
    str(path) for path in Path("src").rglob("*")
    if path.suffix in {".c", ".cu"}
}
if declared != actual_sources:
    errors.append(
        "Makefile/source parity failure: "
        f"missing={sorted(actual_sources - declared)} "
        f"stale={sorted(declared - actual_sources)}"
    )

objects = []
for source in sorted(actual_sources):
    suffix = ".ptx" if source.endswith(".cu") else ".o"
    objects.append("build/obj/" + source.rsplit(".", 1)[0] + suffix)
if len(objects) != len(set(objects)):
    errors.append("source-relative object collision")

contract = Path("AGENTS.md").read_text()
required_contracts = (
    "A new file is not an implementation convenience. It is a new semantic owner",
    "config/source_owners.tsv",
    "maximum of three production",
    "Explicit user authorization is required",
    "source-relative object paths are mandatory",
    "tests/test_architecture_boundaries.sh",
)
for required in required_contracts:
    if required not in contract:
        errors.append(f"AGENTS.md lacks repository contract: {required}")

doc_files = [Path("AGENTS.md"), Path("PROJECT.md"), Path("README.md")]
doc_files.extend(Path("docs").rglob("*.md"))
path_reference = re.compile(r"`((?:src|include|tests|config)/[^` ]+)`")
for document in doc_files:
    for line_number, line in enumerate(
        document.read_text(errors="ignore").splitlines(), 1
    ):
        for reference in path_reference.findall(line):
            reference = reference.rstrip(".,:;")
            if any(token in reference for token in ("*", "[", "<", "{")):
                continue
            if not Path(reference).exists():
                errors.append(
                    f"stale owned path in {document}:{line_number}: {reference}"
                )

if errors:
    raise SystemExit("repository layout:\n  " + "\n  ".join(errors))
print(
    "repository layout: ok "
        f"owned={len(owned)} production={len(production)} "
        "prefix=0 long=0 depth=0 collisions=0 stale_paths=0"
)
PY
