#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

python3 - <<'PY'
from pathlib import Path

required = ("Owner", "Owns", "Does not own", "Invariants", "Boundary")
missing = []
duplicate_owners = []
for root in (Path("src"), Path("include")):
    for path in sorted(root.rglob("*")):
        if path.suffix not in {".c", ".h", ".cu"}:
            continue
        head = "\n".join(path.read_text(errors="ignore").splitlines()[:80])
        absent = [token for token in required if token not in head]
        if absent:
            missing.append(f"{path}: {','.join(absent)}")
        owner_count = sum(
            1 for line in path.read_text(errors="ignore").splitlines()
            if line.strip().startswith("* Owner:")
        )
        if owner_count != 1:
            duplicate_owners.append(f"{path}: {owner_count}")
if missing:
    raise SystemExit("code natural: missing source contracts\n  " + "\n  ".join(missing))
if duplicate_owners:
    raise SystemExit(
        "code natural: source owner contract cardinality\n  "
        + "\n  ".join(duplicate_owners)
    )
print("code natural contracts: ok")
PY

python3 - <<'PY'
from pathlib import Path
import re

rows = {}
for line in Path("config/source_owners.tsv").read_text().splitlines():
    if not line or line.startswith("#"):
        continue
    fields = line.split("\t")
    rows[fields[0]] = fields

call = re.compile(r"\b(?:printf|fprintf|vprintf|vfprintf|puts|fputs|putchar|fputc|perror)\s*\(")
bad = []
for path in sorted(Path("src").rglob("*")):
    if path.suffix not in {".c", ".h", ".cu"}:
        continue
    text = path.read_text(errors="ignore")
    if not call.search(text):
        continue
    name = str(path)
    fields = rows.get(name)
    boundary = fields[5] if fields else ""
    admitted = (name.startswith("src/cli/io/") or
                name == "src/daemon/yvexd.c" or
                boundary in {"transactional-io", "file-serialization"})
    if not admitted:
        bad.append(name)
    if not (name.startswith("src/cli/") or name == "src/daemon/yvexd.c"):
        for source_line in text.splitlines():
            if call.search(source_line) and re.search(r"\b(?:stdout|stderr)\b", source_line):
                bad.append(name + ": direct process stream")
                break
if bad:
    raise SystemExit("code natural: unowned direct output\n  " + "\n  ".join(sorted(set(bad))))
print("code natural output ownership: ok")
PY

if grep -RInE '\b(system|popen|execl|execv)[[:space:]]*\(' \
    src/model src/graph src/runtime src/generation src/backend src/source src/artifact src/gguf; then
  echo "code natural: domain owner shells out" >&2
  exit 1
fi

grep -nF 'e7706faf8d1c3b9f241e36860640ad1dac644ede' src/model/families/deepseek_v4.c >/dev/null
grep -nF 'yvex_attention_execute_supported' src/graph/attention.c >/dev/null
grep -nF 'yvex_attention_hadamard_cpu' src/graph/numeric.c >/dev/null
grep -nF 'yvex_attention_topk_select' src/graph/numeric.c >/dev/null
grep -nF 'const yvex_graph_family_api *yvex_graph_lower_deepseek_v4' src/graph/families/deepseek_v4.c >/dev/null
grep -nF 'graph_plan_build,' src/graph/families/deepseek_v4.c >/dev/null
grep -nF 'yvex_backend_attention_execute' src/backend/cuda/families/deepseek_v4.c >/dev/null
grep -nF 'yvex_test_attention_reference_execute' tests/reference/deepseek_attention.c >/dev/null

if grep -RInE 'attention_execution_supported[[:space:]]*=[[:space:]]*1|attention_cuda_execution_ready[[:space:]]*=[[:space:]]*1|runtime_generation_ready[[:space:]]*=[[:space:]]*1' \
    src tests/live; then
  echo "code natural: paused attention/runtime claim promoted" >&2
  exit 1
fi

sh tests/test_source_ownership.sh >/dev/null
sh tests/test_repository_layout.sh >/dev/null
sh tests/test_architecture_boundaries.sh >/dev/null

echo "code natural: ok contracts=complete direct_output=0 attention_claims=0"
