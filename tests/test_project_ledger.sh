#!/usr/bin/env sh
set -eu

project=PROJECT.md
tmp_base="${TMPDIR:-/tmp}/yvex-project-ledger.$$"
rows="${tmp_base}.rows"
tracks="${tmp_base}.tracks"
expected_tracks="${tmp_base}.expected-tracks"
counts="${tmp_base}.counts"
expected_counts="${tmp_base}.expected-counts"
trap 'rm -f "$rows" "$tracks" "$expected_tracks" "$counts" "$expected_counts"' EXIT

fail() {
  printf 'project ledger: %s\n' "$1" >&2
  exit 1
}

test -f "$project" || fail "missing $project"
test ! -e docs/spine.md || fail "shadow project authority exists: docs/spine.md"

awk -F '|' '
function trim(value) {
  gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
  return value
}
function uncode(value) {
  value = trim(value)
  gsub(/^`|`$/, "", value)
  return value
}
/^## 9\. Complete Track\/Wave Ledger$/ {
  in_ledger = 1
  next
}
/^## 10\. Evidence Lanes$/ {
  in_ledger = 0
  next
}
!in_ledger { next }
/^### 9\.[0-9]+ TRACK\./ {
  track = $0
  sub(/^### 9\.[0-9]+ /, "", track)
  kind = ""
  next
}
/^#### First-Class Milestones$/ {
  kind = "milestone"
  next
}
/^#### Recovered And Subordinate Rows$/ {
  kind = "support"
  next
}
/^\| `[^`]+` \|/ {
  id = uncode($2)
  if (kind == "milestone") {
    rank = "milestone"
    state = uncode($4)
  } else if (kind == "support") {
    rank = uncode($3)
    state = uncode($5)
    consumer = trim($7)
    if (consumer == "") {
      printf "project ledger: missing consumer for %s\n", id > "/dev/stderr"
      exit 1
    }
  } else {
    printf "project ledger: row outside a ledger table: %s\n", id > "/dev/stderr"
    exit 1
  }
  print track "\t" id "\t" rank "\t" state
}
' "$project" > "$rows"

row_count=$(wc -l < "$rows" | tr -d ' ')
test "$row_count" -eq 665 || fail "expected 665 canonical IDs, found $row_count"

unique_count=$(cut -f 2 "$rows" | LC_ALL=C sort -u | wc -l | tr -d ' ')
test "$unique_count" -eq 665 || fail "canonical IDs are not unique: $unique_count/665"

duplicate=$(cut -f 2 "$rows" | LC_ALL=C sort | uniq -d | head -n 1 || true)
test -z "$duplicate" || fail "duplicate canonical ID: $duplicate"

id_hash=$(cut -f 2 "$rows" | LC_ALL=C sort | sha256sum | awk '{ print $1 }')
expected_id_hash=2bfe65bb296ca7d02e276a427ddee765c917a2601d31310758d5dbcb49b623a8
test "$id_hash" = "$expected_id_hash" ||
  fail "canonical ID set changed without an explicit ledger-guard update: $id_hash"

summary_hash=$(awk '
  /^### 7\.1 Per-Track Counts$/ { in_summary = 1; next }
  /^### 7\.2 Stable Track Names$/ { in_summary = 0 }
  in_summary && /^\| `TRACK\./ { print }
' "$project" | sha256sum | awk '{ print $1 }')
expected_summary_hash=2a49f1c1a58e8d1cdb0ba1ed8b0557be21ed9b45936adec7609ae9cf2c66df92
test "$summary_hash" = "$expected_summary_hash" ||
  fail "per-track dashboard changed without a calculated-count update: $summary_hash"

awk -F '\t' '
BEGIN { ok = 1 }
$3 !~ /^(milestone|capability|evidence|subtask|migration|future)$/ {
  printf "project ledger: invalid rank for %s: %s\n", $2, $3 > "/dev/stderr"
  ok = 0
}
$4 !~ /^(complete|active|partial|blocked|planned|reopened|not-measured|deferred|superseded)$/ {
  printf "project ledger: invalid state for %s: %s\n", $2, $4 > "/dev/stderr"
  ok = 0
}
$3 == "milestone" && $4 !~ /^(complete|active|partial|blocked|planned|not-measured)$/ {
  printf "project ledger: invalid milestone state for %s: %s\n", $2, $4 > "/dev/stderr"
  ok = 0
}
$3 == "capability" && $4 != "complete" { ok = 0 }
$3 == "evidence" && $4 !~ /^(complete|reopened)$/ { ok = 0 }
$3 == "subtask" && $4 != "planned" { ok = 0 }
$3 == "migration" && $4 != "superseded" { ok = 0 }
$3 == "future" && $4 != "deferred" { ok = 0 }
$4 == "active" && $3 != "milestone" { ok = 0 }
END { exit ok ? 0 : 1 }
' "$rows" || fail "rank/state contract violation"

state_count() {
  awk -F '\t' -v wanted="$1" '$4 == wanted { count++ } END { print count + 0 }' "$rows"
}

for expected in \
  complete:176 \
  active:1 \
  partial:1 \
  blocked:31 \
  planned:434 \
  reopened:2 \
  deferred:9 \
  superseded:10 \
  not-measured:1
do
  state=${expected%%:*}
  count=${expected##*:}
  actual=$(state_count "$state")
  test "$actual" -eq "$count" || fail "state $state expected $count, found $actual"
done

milestone_count=$(awk -F '\t' '$3 == "milestone" { count++ } END { print count + 0 }' "$rows")
test "$milestone_count" -eq 37 || fail "expected 37 milestones, found $milestone_count"

active_id=$(awk -F '\t' '$4 == "active" { print $2 }' "$rows")
test "$active_id" = V010.DOCS.ARCHITECTURE.0 || fail "unexpected active milestone: $active_id"

active_lines=$(grep -c '^Active Next: ' "$project")
test "$active_lines" -eq 1 || fail "expected one machine-readable Active Next, found $active_lines"
project_active=$(sed -n 's/^Active Next: \([^[:space:]]*\)$/\1/p' "$project")
test "$project_active" = "$active_id" ||
  fail "Active Next does not match active milestone: $project_active/$active_id"

grep '^### 9\.[0-9][0-9]* TRACK\.' "$project" |
  sed 's/^### 9\.[0-9][0-9]* //' > "$tracks"

cat > "$expected_tracks" <<'EOF'
TRACK.SCOPE
TRACK.SOURCE
TRACK.MAP
TRACK.QUANT
TRACK.ARTIFACT
TRACK.INTEGRITY
TRACK.MODEL
TRACK.TENSOR
TRACK.RESIDENCY
TRACK.BACKEND
TRACK.GRAPH
TRACK.PREFILL
TRACK.KV
TRACK.DECODE
TRACK.LOGITS
TRACK.SAMPLING
TRACK.TOKENIZER
TRACK.GENERATION
TRACK.OPERATOR
TRACK.SERVE
TRACK.EVAL
TRACK.BENCH
TRACK.RELEASE
TRACK.POST010
EOF

cmp -s "$tracks" "$expected_tracks" || fail "stable 24-track order changed"

awk -F '\t' '{ count[$1]++ } END { for (track in count) print track, count[track] }' "$rows" |
  LC_ALL=C sort > "$counts"

cat > "$expected_counts" <<'EOF'
TRACK.ARTIFACT 16
TRACK.BACKEND 30
TRACK.BENCH 17
TRACK.DECODE 16
TRACK.EVAL 16
TRACK.GENERATION 54
TRACK.GRAPH 75
TRACK.INTEGRITY 15
TRACK.KV 22
TRACK.LOGITS 19
TRACK.MAP 13
TRACK.MODEL 23
TRACK.OPERATOR 82
TRACK.POST010 23
TRACK.PREFILL 28
TRACK.QUANT 6
TRACK.RELEASE 42
TRACK.RESIDENCY 43
TRACK.SAMPLING 16
TRACK.SCOPE 29
TRACK.SERVE 12
TRACK.SOURCE 26
TRACK.TENSOR 28
TRACK.TOKENIZER 14
EOF

cmp -s "$counts" "$expected_counts" || fail "per-track canonical counts changed"

for required_id in \
  MODEL.CLASS.QWEN.0 \
  MODEL.CLASS.GEMMA.0 \
  TENSOR.COLLECTION.QWEN.0 \
  TENSOR.COLLECTION.GEMMA.0 \
  OWI.TARGETS.QWEN.0 \
  OWI.TARGETS.GEMMA.0 \
  MODELS.SOURCE.MAP.HANDOFF.0 \
  V010.GGUF.QTYPE.ABI.0 \
  V010.GGUF.ARTIFACT.ABI.0 \
  V010.PROJECT.RECOVERY.1
do
  grep -F "$(printf '\t%s\t' "$required_id")" "$rows" >/dev/null ||
    fail "missing required recovered ID: $required_id"
done

grep -F 'Qwen, Gemma, and dense/common work already implemented remains active' "$project" >/dev/null ||
  fail "multi-family engineering scope is missing"
grep -F 'DeepSeek-V4-Flash is the only model whose complete source-to-text chain closes' "$project" >/dev/null ||
  fail "exclusive v0.1 release target is missing"
grep -F 'total canonical IDs: **665**' "$project" >/dev/null ||
  fail "declared global ledger total is missing"

echo "project ledger: ok (tracks=24 recovered=631 ids=665 milestones=37 active=$active_id)"
