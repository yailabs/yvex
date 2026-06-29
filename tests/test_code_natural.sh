#!/usr/bin/env sh
set -eu

impl_files="$(git ls-files '*.c' '*.cu' | grep -v '^tests/' | grep -v '^build/' || true)"

git grep -n \
  -e 'compressed implementation unit' \
  -e 'inlined yvex_' \
  -e '===== yvex_' \
  -e '===== gguf/' \
  -e '===== models/' \
  -- '*.c' '*.h' cuda gguf && {
    echo "mechanical source markers found"
    exit 1
  } || true

git grep -n '^#ifndef .*_H$' -- $impl_files && {
    echo "header guard found inside C implementation"
    exit 1
  } || true

awk '
function reset_file() {
    seen_code = 0
    in_block_comment = 0
}

FNR == 1 {
    reset_file()
}

{
    line = $0

    if (in_block_comment) {
        if (line ~ /\*\//) {
            in_block_comment = 0
        }
        next
    }

    if (line ~ /^[[:space:]]*$/) {
        next
    }
    if (line ~ /^[[:space:]]*\/\*/) {
        if (line !~ /\*\//) {
            in_block_comment = 1
        }
        next
    }
    if (line ~ /^[[:space:]]*\/\//) {
        next
    }
    if (line ~ /^#include/) {
        if (seen_code) {
            print FILENAME ":" FNR ": include after code"
            bad = 1
        }
        next
    }
    if (line ~ /^#/) {
        next
    }

    seen_code = 1
}

END {
    exit bad ? 1 : 0
}
' \
  $impl_files

test ! -f yvex_internal.h

awk '
FNR == 1 {
    in_header = 0
    checked = 0
}
FNR <= 24 && checked == 0 {
    if ($0 ~ /^[[:space:]]*\/\*/) {
        in_header = 1
    }
    if (in_header && $0 ~ /command implementations|helper functions|miscellaneous|glue|stuff|TODO|future implementation goes here|placeholder/) {
        print FILENAME ":" FNR ": vague file header text"
        bad = 1
    }
    if (in_header && $0 ~ /\*\//) {
        checked = 1
        in_header = 0
    }
}
END {
    exit bad ? 1 : 0
}
' \
  $impl_files

if grep -nE 'implemented|ready|supports generation|benchmark result|token/sec|evaluation suite implemented|decode implemented|sampling implemented|generation implemented' \
    yvex_decode.c yvex_logits.c yvex_sampling.c yvex_generation.c yvex_eval.c yvex_bench.c; then
  echo "future boundary files must not claim runtime readiness"
  exit 1
fi

scan_forbidden_claim() {
  pattern=$1
  if git grep -nF "$pattern" -- README.md MODEL_ARTIFACTS.md docs '*.c' include tests; then
    echo "forbidden capability claim found: $pattern"
    exit 1
  fi
}

pattern='GLM sup''port'
scan_forbidden_claim "$pattern"
pattern='GLM generation rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM execution rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM runtime rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM inference imple''mented'
scan_forbidden_claim "$pattern"
pattern='YVEX supports GLM gener''ation'
scan_forbidden_claim "$pattern"
pattern='external GGUF satisfies OW''I'
scan_forbidden_claim "$pattern"
pattern='external runner proves YV''EX'
scan_forbidden_claim "$pattern"
pattern='model target is capab''ility'
scan_forbidden_claim "$pattern"
pattern='full inference imple''mented'
scan_forbidden_claim "$pattern"
pattern='transformer inference rea''dy'
scan_forbidden_claim "$pattern"
pattern='DeepSeek block rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM block rea''dy'
scan_forbidden_claim "$pattern"
pattern='generation rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='decode rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='logits rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='first token gener''ated'
scan_forbidden_claim "$pattern"
pattern='token/sec'':'
scan_forbidden_claim "$pattern"
pattern='benchmark pa''ss'
scan_forbidden_claim "$pattern"

echo "code naturalness: ok"
