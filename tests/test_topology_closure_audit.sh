#!/usr/bin/env sh
set -eu

tmp="${TMPDIR:-/tmp}/yvex-topology-closure-audit.$$"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT

grep_count() {
  file=$1
  if test -f "$file"; then
    wc -l < "$file" | tr -d ' '
  else
    printf '0\n'
  fi
}

run_grep() {
  out=$1
  shift
  git grep -nE "$@" > "$out" || true
}

run_grep "$tmp/direct-output.txt" \
  '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' \
  -- src include
run_grep "$tmp/cli-shaped.txt" \
  '\b(argc|argv)\b|usage:|--help|--output|--audit' \
  -- src include
run_grep "$tmp/cli-includes.txt" \
  '#include "yvex_(cli|operator|console)|#include <.*cli.*>' \
  -- src include
run_grep "$tmp/renderer-leakage.txt" \
  'yvex_backend|yvex_graph_build|yvex_model_ref|native_weight|safetensors|fopen|open\(|read\(|write\(' \
  -- src/cli/render
run_grep "$tmp/text-buffer.txt" \
  'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer|capture|sink' \
  -- src include
run_grep "$tmp/compat-shell.txt" \
  'compat|legacy|shim|bridge|runner|not-bound|renderer-only|_render_boundary|boundary anchor' \
  -- src include

find src -name '*.c' -print0 | xargs -0 wc -l | sort -n > "$tmp/large-files.txt"
grep -nE 'src/cli/(input|commands|model_artifacts|render|io)' Makefile > "$tmp/makefile-cli.txt" || true

allowed_direct="$(
  awk -F: '
    /^src\/cli\/io\// { allowed++ ; next }
    /^src\/io\// { allowed++ ; next }
    /^src\/model\/target\/yvex_model_target_sidecar_write\.c:/ { allowed++ ; next }
    /^src\/model\/artifacts\/yvex_model_artifact_write\.c:/ { allowed++ ; next }
    /^src\/core\/yvex_file_writer\.c:/ { allowed++ ; next }
    END { print allowed + 0 }
  ' "$tmp/direct-output.txt"
)"
direct_total="$(grep_count "$tmp/direct-output.txt")"
direct_other="$(
  awk -F: '
    !($1 ~ /^src\/cli\/io\// ||
      $1 ~ /^src\/io\// ||
      $1 == "src/model/target/yvex_model_target_sidecar_write.c" ||
      $1 == "src/model/artifacts/yvex_model_artifact_write.c" ||
      $1 == "src/core/yvex_file_writer.c") { other++ }
    END { print other + 0 }
  ' "$tmp/direct-output.txt"
)"
cli_input_total="$(grep_count "$tmp/cli-shaped.txt")"
cli_input_other="$(
  awk -F: '
    !($1 ~ /^src\/cli\/input\// ||
      $1 ~ /^src\/cli\/commands\// ||
      $1 ~ /^src\/cli\/model_artifacts\// ||
      $1 ~ /^src\/cli\/catalog\// ||
      $1 == "src/cli/yvex_cli.c") { other++ }
    END { print other + 0 }
  ' "$tmp/cli-shaped.txt"
)"
cli_include_other="$(
  awk -F: '
    !($1 ~ /^src\/cli\// || $1 ~ /^src\/core\//) { other++ }
    END { print other + 0 }
  ' "$tmp/cli-includes.txt"
)"
large_warning="$(
  awk '$1 ~ /^[0-9]+$/ && $1 > 1000 { c++ } END { print c + 0 }' "$tmp/large-files.txt"
)"
large_strong="$(
  awk '$1 ~ /^[0-9]+$/ && $1 > 2500 { c++ } END { print c + 0 }' "$tmp/large-files.txt"
)"
large_critical="$(
  awk '$1 ~ /^[0-9]+$/ && $1 > 5000 { c++ } END { print c + 0 }' "$tmp/large-files.txt"
)"
command_over="$(
  find src/cli/commands -name '*.c' -print0 |
    xargs -0 wc -l |
    awk '$2 != "total" && $1 ~ /^[0-9]+$/ && $1 > 350 { c++ } END { print c + 0 }'
)"
lib_cli_leak="$(
  awk '
    /^CORE_SRCS :=/ { in_core = 1 }
    /^CUDA_CU_SRCS :=/ { in_core = 0 }
    in_core && /src\/cli\/(input|commands|model_artifacts|render|io)/ { c++ }
    END { print c + 0 }
  ' Makefile
)"

printf 'topology closure audit: advisory\n'
printf 'direct_output_total: %s\n' "$direct_total"
printf 'direct_output_allowed: %s\n' "$allowed_direct"
printf 'direct_output_legacy_or_violation: %s\n' "$direct_other"
printf 'cli_shaped_total: %s\n' "$cli_input_total"
printf 'cli_shaped_outside_cli: %s\n' "$cli_input_other"
printf 'cli_operator_include_outside_cli_core: %s\n' "$cli_include_other"
printf 'renderer_leakage_matches: %s\n' "$(grep_count "$tmp/renderer-leakage.txt")"
printf 'report_text_buffer_matches: %s\n' "$(grep_count "$tmp/text-buffer.txt")"
printf 'compat_shell_matches: %s\n' "$(grep_count "$tmp/compat-shell.txt")"
printf 'command_adapters_over_350: %s\n' "$command_over"
printf 'large_files_over_1000: %s\n' "$large_warning"
printf 'large_files_over_2500: %s\n' "$large_strong"
printf 'large_files_over_5000: %s\n' "$large_critical"
printf 'libyvex_cli_leakage: %s\n' "$lib_cli_leak"

test ! -f src/model/target/yvex_model_target_runner.c

if git grep -nE '\b(argc|argv)\b' -- \
    src/model/target/*.c \
    src/model/target/*.h \
    src/cli/render/yvex_model_target_render.c \
    src/cli/render/yvex_model_target_render.h; then
  echo "model-target domain/render must not carry CLI-shaped argc/argv"
  exit 1
fi

if git grep -nE 'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer' -- \
    src/model/target \
    src/cli/render/yvex_model_target_render.c \
    src/cli/render/yvex_model_target_render.h; then
  echo "model-target reports/renderers must not use pre-rendered text buffers"
  exit 1
fi

if git grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' -- \
    src/model/target/*.c \
    src/model/target/*.h; then
  echo "model-target domain/report must not include CLI/operator headers"
  exit 1
fi

echo "topology closure audit: model-target hard guards ok"

model_artifacts_root_lines="$(wc -l < src/model/yvex_model_artifacts.c | tr -d ' ')"
if test "$model_artifacts_root_lines" -gt 500; then
  echo "model-artifacts root remains a monolith"
  exit 1
fi

if git grep -nE '#include "yvex_(cli|operator|console)|yvex_operator_render|#include <.*cli.*>' -- \
    src/model/yvex_model_artifacts.c \
    src/model/artifacts/*.c \
    src/model/artifacts/*.h; then
  echo "model-artifacts domain/report must not include CLI/operator headers"
  exit 1
fi

if git grep -nE '\b(argc|argv)\b' -- \
    src/model/yvex_model_artifacts.c \
    src/model/artifacts/*.c \
    src/model/artifacts/*.h \
    src/cli/render/yvex_model_artifacts_render.c \
    src/cli/render/yvex_model_artifacts_render.h; then
  echo "model-artifacts domain/render must not carry CLI-shaped input"
  exit 1
fi

if git grep -nE 'usage:|--help|--output|--audit' -- \
    src/model/yvex_model_artifacts.c \
    src/model/artifacts/*.c \
    src/model/artifacts/*.h; then
  echo "model-artifacts domain/report must not carry command grammar"
  exit 1
fi

if git grep -nE 'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer' -- \
    src/model/artifacts \
    src/cli/render/yvex_model_artifacts_render.c \
    src/cli/render/yvex_model_artifacts_render.h; then
  echo "model-artifacts reports/renderers must not use text-buffer report debt"
  exit 1
fi

model_artifacts_command_lines="$(wc -l < src/cli/commands/yvex_model_artifacts_cli.c | tr -d ' ')"
if test "$model_artifacts_command_lines" -gt 350; then
  echo "model-artifacts command adapter remains too large"
  exit 1
fi

grep -nF 'yvex_model_artifacts_args_parse' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifact_report_build' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifacts_render' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null

if git grep -nE 'yvex_operator_private|yvex_operator_render_private|write_escaped|write_field|write_u64_field|model_artifacts_cli_strdup|path_exists|is_path_like_reference|set_path_ref|yvex_model_registry|yvex_model_ref|yvex_model_gate|yvex_artifact|yvex_backend|yvex_weight_table|native_weight|safetensors|mkdir|access\(|opendir|readdir|poll|waitpid|signal|printf|fprintf|fwrite' -- \
    src/cli/commands/yvex_model_artifacts_cli.c; then
  echo "model-artifacts command adapter must stay thin"
  exit 1
fi

echo "topology closure audit: model-artifacts hard guards ok"
