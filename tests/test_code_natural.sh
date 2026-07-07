#!/usr/bin/env sh
set -eu

impl_files="$(git ls-files '*.c' '*.cu' | grep -v '^tests/' | grep -v '^build/' || true)"

git grep -n \
  -e 'compressed implementation unit' \
  -e 'inlined yvex_' \
  -e '===== yvex_' \
  -e '===== src/gguf/' \
  -e '===== models/' \
  -- '*.c' '*.h' src && {
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
test -z "$(git ls-files 'yvex_*.c')"
test -z "$(git ls-files 'yvex_*_private.h')"

if grep -nF 'usage: yvex model-target' src/model/yvex_model.c; then
  echo "model-target usage text must not live in src/model/yvex_model.c"
  exit 1
fi

if grep -nF 'yvex model-target' src/model/yvex_model.c; then
  echo "model-target command text must not live in src/model/yvex_model.c"
  exit 1
fi

grep -nF 'usage: yvex model-target' src/cli/commands/yvex_model_target_cli.c >/dev/null

DOMAIN_FILES='src/accounts src/artifact src/backend src/core src/daemon src/gguf src/graph src/metrics src/model src/runtime src/source src/tokenizer src/generation'

PRINT_HITS="$(
  grep -RInE '\b(printf|vprintf|puts|putchar|perror)\s*\(' $DOMAIN_FILES || true
)"

if test -n "$PRINT_HITS"; then
  echo "$PRINT_HITS"
  echo "domain files must not use obvious direct operator print helpers"
  exit 1
fi

USAGE_HITS="$(
  grep -RIn 'usage: yvex' $DOMAIN_FILES || true
)" || true

if test -n "$USAGE_HITS"; then
  echo "$USAGE_HITS"
  echo "usage text outside src/cli"
  exit 1
fi

OPTION_HITS="$(
  grep -RInE '\bargc\b|\bargv\b|strcmp\(argv\[|--output|--audit|--json|--include-' $DOMAIN_FILES || true
)" || true

if test -n "$OPTION_HITS"; then
  echo "$OPTION_HITS"
  echo "domain files must not expose CLI parsing tokens"
  exit 1
fi

CATALOG_HITS="$(
  git grep -n 'src/cli/catalog' -- 'src/**/*.c' 'src/**/*.h' |
  grep -vE '^src/cli/'
)" || true

if test -n "$CATALOG_HITS"; then
  echo "$CATALOG_HITS"
  echo "CLI catalog included outside src/cli"
  exit 1
fi

CATALOG_LOGIC_HITS="$(
  grep -RInE '\b(if|for|while|switch|return)\b|[{}();]|printf|fprintf' src/cli/catalog || true
)"

if test -n "$CATALOG_LOGIC_HITS"; then
  echo "$CATALOG_LOGIC_HITS"
  echo "CLI catalog files must contain lists only"
  exit 1
fi

PLACEHOLDER_RENDER_HITS="$(
  git grep -nE 'renderer-only|const void \*report|present/not-bound|not-bound|_render_boundary' -- \
    'src/cli/render/**' || true
)"
if test -n "$PLACEHOLDER_RENDER_HITS"; then
  echo "$PLACEHOLDER_RENDER_HITS"
  echo "placeholder renderers are forbidden"
  exit 1
fi

grep -nF 'const yvex_source_report *report' src/cli/render/yvex_source_render.c >/dev/null
grep -nF 'yvex_cli_out_writef' src/cli/render/yvex_source_render.c >/dev/null
if grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_source_render.c; then
  echo "source renderer must use src/cli/io writers"
  exit 1
fi

if grep -RInE '#include .*src/cli|#include "yvex_(cli|console|render)' src/source; then
  echo "source cell must not include CLI headers"
  exit 1
fi

COMMAND_STRUCT_HITS="$(
  grep -RInE '^(struct yvex_graph|struct yvex_plan|struct yvex_memory_plan|struct yvex_native_weight_table|struct yvex_backend|struct yvex_engine|struct yvex_kv|struct yvex_logits|struct yvex_sampling)' src/cli/commands || true
)"
if test -n "$COMMAND_STRUCT_HITS"; then
  echo "$COMMAND_STRUCT_HITS"
  echo "domain structs must not live in src/cli/commands"
  exit 1
fi

COMMAND_EXPORT_HITS="$(
  grep -RInE '^(int|void|const char \*) yvex_(account|artifact|backend|engine|graph|kv|logits|sampling|source|native|gguf|conversion|metrics|trace|model_registry|model_ref|tokenizer|prompt|tokens|decode|generation)' src/cli/commands |
  grep -vE '_(command|help)\(' || true
)"
if test -n "$COMMAND_EXPORT_HITS"; then
  echo "$COMMAND_EXPORT_HITS"
  echo "domain exported symbols must not live in src/cli/commands"
  exit 1
fi

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

awk '
FNR == 1 {
    in_comment = 0
}
{
    line = $0
    comment_line = 0

    if (in_comment) {
        comment_line = 1
        if (line ~ /\*\//) {
            in_comment = 0
        }
    } else if (line ~ /^[[:space:]]*\/\*/) {
        comment_line = 1
        if (line !~ /\*\//) {
            in_comment = 1
        }
    } else if (line ~ /^[[:space:]]*\/\//) {
        comment_line = 1
    }

    if (!comment_line) {
        next
    }

    if (line ~ /handles things|does stuff|helper function|process data|business logic|simple wrapper around everything/) {
        print FILENAME ":" FNR ": vague source comment text"
        bad = 1
    }
    if (line ~ /magic/ && line !~ /GGUF magic|YVEX_GGUF_MAGIC|bad GGUF magic/) {
        print FILENAME ":" FNR ": vague source comment text"
        bad = 1
    }
}
END {
    exit bad ? 1 : 0
}
' \
  $impl_files

for f in \
  src/model/yvex_model.c \
  src/cli/commands/yvex_model_target_cli.c \
  src/cli/yvex_cli.c \
  src/cli/commands/yvex_runtime_cli.c \
  src/cli/commands/yvex_generate_cli.c \
  src/cli/commands/yvex_artifact_cli.c \
  src/cli/commands/yvex_source_cli.c
do
  grep -nF 'Purpose:' "$f" >/dev/null
  grep -nF 'Inputs:' "$f" >/dev/null
  grep -nF 'Effects:' "$f" >/dev/null
  grep -nF 'Failure:' "$f" >/dev/null
  grep -nF 'Boundary:' "$f" >/dev/null
done

grep -nF 'command grammar' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'usage/help' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'report rendering' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'does not create capability' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'model metadata/materialization facts are not model support' src/model/yvex_model.c >/dev/null
grep -nF 'tensor payload bytes' src/model/yvex_model.c >/dev/null
grep -nF 'materialization' src/model/yvex_model.c >/dev/null

if grep -nE 'implemented|ready|supports generation|benchmark result|token/sec|evaluation suite implemented|generation implemented' \
    src/cli/commands/yvex_generate_cli.c src/eval/yvex_eval.c src/bench/yvex_bench.c; then
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
pattern='Qwen source down''loaded'
scan_forbidden_claim "$pattern"
pattern='Qwen source veri''fied'
scan_forbidden_claim "$pattern"
pattern='Qwen suppor''ted'
scan_forbidden_claim "$pattern"
pattern='Qwen runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen CUDA runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma source down''loaded'
scan_forbidden_claim "$pattern"
pattern='Gemma source veri''fied'
scan_forbidden_claim "$pattern"
pattern='native inventory implemented for Qw''en'
scan_forbidden_claim "$pattern"
pattern='native inventory implemented for Gem''ma'
scan_forbidden_claim "$pattern"
pattern='native inventory proves source rea''dy'
scan_forbidden_claim "$pattern"
pattern='native_inventory_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source tensor roles map''ped'
scan_forbidden_claim "$pattern"
pattern='tensor roles map''ped'
scan_forbidden_claim "$pattern"
pattern='Qwen model class imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen model class rea''dy'
scan_forbidden_claim "$pattern"
pattern='model_class_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='Gemma model class imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma CUDA runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Metal support imple''mented'
scan_forbidden_claim "$pattern"
pattern='CUDA inference rea''dy'
scan_forbidden_claim "$pattern"
pattern='CUDA generation rea''dy'
scan_forbidden_claim "$pattern"
pattern='CUDA runtime rea''dy'
scan_forbidden_claim "$pattern"
pattern='FlashAttention imple''mented'
scan_forbidden_claim "$pattern"
pattern='tensor core GEMM imple''mented'
scan_forbidden_claim "$pattern"
pattern='source_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source_manifest_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='metadata_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_identity_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_hash_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_remote_chec''ked: true'
scan_forbidden_claim "$pattern"
pattern='payload_loa''ded: true'
scan_forbidden_claim "$pattern"
pattern='native_safetensors_payload_loa''ded: true'
scan_forbidden_claim "$pattern"
pattern='generation_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: measu''red'
scan_forbidden_claim "$pattern"
pattern='benchmark mea''sured'
scan_forbidden_claim "$pattern"
pattern='release_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='20 tok/s achie''ved'
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
pattern='release_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='runtime_claim: sup''ported'
scan_forbidden_claim "$pattern"
pattern='generation: sup''ported'
scan_forbidden_claim "$pattern"
pattern='full_model_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='real_deepseek_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: meas''ured'
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
pattern='DeepSeek CUDA generation imple''mented'
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
pattern='qwen_runtime_status: sup''ported'
scan_forbidden_claim "$pattern"
pattern='metal_backend_status: sup''ported'
scan_forbidden_claim "$pattern"
pattern='Qwen generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Metal support imple''mented'
scan_forbidden_claim "$pattern"
pattern='provider generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='raw_token_stored_by_yvex: tr''ue'
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

if awk '
  /\b(system|popen|execl|execv|fork)[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
    bad = 1
  }
  END {
    exit bad ? 1 : 0
  }
' src/cli/commands/yvex_model_artifacts_cli.c; then
  :
else
  echo "models command owners must not shell out; provider execution belongs to accounts"
  exit 1
fi

if grep -nE '\b(system|popen|execl)[[:space:]]*\(' src/cli/commands/yvex_accounts_cli.c; then
  echo "accounts must use bounded exec helpers only"
  exit 1
fi

runtime_files="$(git ls-files 'src/cli/commands/yvex_runtime_cli.c' 'src/cli/commands/yvex_backend_cli.c' 'src/cli/commands/yvex_tokenizer_cli.c' 'src/tokenizer/yvex_token_input.c' 'src/cli/commands/yvex_kv_cli.c' 'src/generation/yvex_prefill.c' 'src/cli/commands/yvex_paths_cli.c' 2>/dev/null || true)"

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
