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

if grep -nE 'implemented|ready|supports generation|benchmark result|token/sec|evaluation suite implemented|generation implemented' \
    yvex_generation.c yvex_eval.c yvex_bench.c; then
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
pattern='full transformer prefill imple''mented'
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
pattern='models check imple''mented'
scan_forbidden_claim "$pattern"
pattern='graph check imple''mented'
scan_forbidden_claim "$pattern"
pattern='DeepSeek generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='prepare material''izes'
scan_forbidden_claim "$pattern"
pattern='prepare runs gr''aph'
scan_forbidden_claim "$pattern"
pattern='prepare starts ser''ver'
scan_forbidden_claim "$pattern"
pattern='prepare runs ch''at'
scan_forbidden_claim "$pattern"
pattern='prepare dec''odes'
scan_forbidden_claim "$pattern"
pattern='prepare produces log''its'
scan_forbidden_claim "$pattern"
pattern='prepare sam''ples'
scan_forbidden_claim "$pattern"
pattern='prepare gener''ates'
scan_forbidden_claim "$pattern"
pattern='prepare bench''marks'
scan_forbidden_claim "$pattern"
pattern='check material''izes as generation'
scan_forbidden_claim "$pattern"
pattern='check runs gr''aph as generation'
scan_forbidden_claim "$pattern"
pattern='check produces log''its'
scan_forbidden_claim "$pattern"
pattern='check gener''ates'
scan_forbidden_claim "$pattern"
pattern='20 tok''/s achie''ved'
scan_forbidden_claim "$pattern"
pattern='DSpark par''ity'
scan_forbidden_claim "$pattern"
pattern='decode_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='logits_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='sampling_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='generation_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='full_model_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='real_deepseek_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: mea''sured'
scan_forbidden_claim "$pattern"
pattern='provider generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='streaming generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='execution_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='graph_execution_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='inference_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='full_transformer_prefill_ready: tr''ue'
scan_forbidden_claim "$pattern"
pattern='operator path gener''ates'
scan_forbidden_claim "$pattern"
pattern='paths configure down''loads'
scan_forbidden_claim "$pattern"
pattern='paths configure creates art''ifacts'
scan_forbidden_claim "$pattern"
pattern='target paths prepare mod''els'
scan_forbidden_claim "$pattern"
pattern='target paths check mod''els'
scan_forbidden_claim "$pattern"
pattern='target paths create art''ifacts'
scan_forbidden_claim "$pattern"
pattern='target paths register ali''ases'
scan_forbidden_claim "$pattern"
pattern='target paths inspect safeten''sors'
scan_forbidden_claim "$pattern"
pattern='target paths inspect GG''UF'
scan_forbidden_claim "$pattern"

if grep -nE '\b(system|popen|execl|execv|fork)[[:space:]]*\(' yvex_model_artifacts.c; then
  echo "models prepare/check must not shell out"
  exit 1
fi

runtime_files="$(git ls-files 'yvex_runtime*.c' 'yvex_backend.c' 'yvex_tokenizer.c' 'yvex_token_input.c' 'yvex_kv.c' 'yvex_prefill.c' 'yvex_fs.c' 2>/dev/null || true)"

if [ -n "$runtime_files" ] && grep -nE '\b(system|popen|execl|execv|fork)[[:space:]]*\(' $runtime_files; then
  echo "runtime ownership files must not shell out"
  exit 1
fi

scan_runtime_claim() {
  pattern=$1
  if [ -n "$runtime_files" ] && git grep -nF "$pattern" -- $runtime_files; then
    echo "forbidden runtime claim found: $pattern"
    exit 1
  fi
}

pattern='execution_ready: tr''ue'
scan_runtime_claim "$pattern"
pattern='generation_ready: tr''ue'
scan_runtime_claim "$pattern"
pattern='generation rea''dy'
scan_runtime_claim "$pattern"
pattern='inference rea''dy'
scan_runtime_claim "$pattern"
pattern='DeepSeek generation imple''mented'
scan_runtime_claim "$pattern"

echo "code naturalness: ok"
