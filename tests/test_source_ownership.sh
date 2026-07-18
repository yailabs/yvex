#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

python3 - <<'PY'
from collections import Counter, defaultdict
from pathlib import Path
import csv
import re
import sys

manifest = Path("config/source_owners.tsv")
if not manifest.is_file():
    raise SystemExit("source ownership: missing config/source_owners.tsv")

actual = sorted(
    str(path)
    for root in (Path("src"), Path("include"))
    for path in root.rglob("*")
    if path.suffix in {".c", ".h", ".cu"}
)

rows = []
with manifest.open(newline="") as stream:
    for raw in stream:
        if raw.startswith("#") or not raw.strip():
            continue
        fields = next(csv.reader([raw], delimiter="\t"))
        if len(fields) != 9:
            raise SystemExit(f"source ownership: expected 9 fields: {raw.rstrip()}")
        rows.append(fields)

paths = [row[0] for row in rows]
if sorted(paths) != actual:
    missing = sorted(set(actual) - set(paths))
    stale = sorted(set(paths) - set(actual))
    raise SystemExit(
        "source ownership: manifest parity failure\n"
        f"  missing={missing}\n  stale={stale}"
    )
duplicates = [path for path, count in Counter(paths).items() if count != 1]
if duplicates:
    raise SystemExit(f"source ownership: duplicate paths: {duplicates}")

allowed_scope = {"generic", "family", "backend", "entrypoint"}
allowed_visibility = {"public", "private"}
allowed_partition = {"interface", "implementation", "backend-kernel", "entrypoint"}
owner_partitions = Counter()
owner_files = defaultdict(list)
for path, subsystem, owner, scope, visibility, boundary, consumers, partition, exception in rows:
    if not subsystem or not owner or not boundary or not consumers:
        raise SystemExit(f"source ownership: incomplete row: {path}")
    if scope not in allowed_scope or visibility not in allowed_visibility:
        raise SystemExit(f"source ownership: invalid scope/visibility: {path}")
    if partition not in allowed_partition:
        raise SystemExit(f"source ownership: invalid partition: {path}")
    if exception != "none":
        raise SystemExit(f"source ownership: unadmitted exception: {path}: {exception}")
    owner_partitions[(owner, partition)] += 1
    owner_files[owner].append(path)

bad_partitions = [key for key, count in owner_partitions.items() if count != 1]
if bad_partitions:
    raise SystemExit(f"source ownership: duplicate owner partitions: {bad_partitions}")
bad_owners = {owner: files for owner, files in owner_files.items() if len(files) > 2}
if bad_owners:
    raise SystemExit(f"source ownership: fragmented semantic owners: {bad_owners}")

family_rows = [row for row in rows if row[3] == "family"]
for row in rows:
    path, subsystem, _owner, scope, _visibility, _boundary, _consumers, _partition, _exception = row
    in_family_path = "/families/" in path
    if in_family_path != (scope == "family"):
        raise SystemExit(f"source ownership: family scope/path mismatch: {path}")
    if scope == "family":
        if not re.match(r"^src/(model|graph|backend/cuda)/families/[^/]+\.(c|cu)$", path):
            raise SystemExit(f"source ownership: invalid family owner path: {path}")
        if re.search(r"_(plan|execute|sink|numeric|internal|reference|report)\.(c|cu)$", path):
            raise SystemExit(f"source ownership: family phase fragmentation: {path}")

families = defaultdict(list)
family_subsystems = Counter()
for row in family_rows:
    path = row[0]
    family = Path(path).stem
    families[family].append(path)
    family_subsystems[(family, row[1])] += 1
for family, files in families.items():
    if len(files) > 3:
        raise SystemExit(f"source ownership: family budget exceeded: {family}: {files}")
for key, count in family_subsystems.items():
    if count > 1:
        raise SystemExit(f"source ownership: multiple family files in subsystem: {key}")

include_roots = [
    Path("."), Path("include"), Path("src/core"), Path("src/cli"),
    Path("src/cli/input"), Path("src/cli/io"), Path("src/cli/model_artifacts"),
    Path("src/cli/render"), Path("src/source"), Path("src/io"),
    Path("src/backend"), Path("src/backend/cuda"), Path("src/runtime"),
    Path("src/server"), Path("src/gguf"), Path("src/generation"),
    Path("src/graph"), Path("src/model/artifacts"),
    Path("src/model/compilation"), Path("src/model/target"),
]
production = [Path(path) for path in actual if path.startswith("src/")]
consumers = defaultdict(set)
include_re = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]')
root = Path.cwd().resolve()
for source in production:
    for line in source.read_text(errors="ignore").splitlines():
        match = include_re.match(line)
        if not match:
            continue
        name = match.group(1)
        candidates = [source.parent / name] + [base / name for base in include_roots]
        for candidate in candidates:
            if not candidate.exists():
                continue
            try:
                target = str(candidate.resolve().relative_to(root))
            except ValueError:
                continue
            consumers[target].add(str(source))
            break

by_path = {row[0]: row for row in rows}
for header in sorted(path for path in actual if path.startswith("src/") and path.endswith(".h")):
    row = by_path[header]
    if len(consumers[header]) >= 2:
        continue
    if row[3] == "backend" and row[5] == "platform-interface":
        continue
    raise SystemExit(
        f"source ownership: private header lacks two production consumers: "
        f"{header}: {sorted(consumers[header])}"
    )

print(
    "source ownership: ok "
    f"files={len(rows)} owners={len(owner_files)} "
    f"family_files={len(family_rows)} exceptions=0"
)
PY
