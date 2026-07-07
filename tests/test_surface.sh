#!/usr/bin/env sh
set -eu

test -f tests/test.c
test -f tests/test_cuda.c
test -f tests/cli.sh
test -f tests/test.h
test -d tests/fixtures
test -d tests/vectors
test -f tests/vectors/manifest.json
test -f tests/vectors/tokenizer.jsonl
test -f tests/vectors/gguf.jsonl
test -f tests/vectors/cuda.jsonl

test -f tests/test_code_natural.sh
test -f tests/test_docs_surface.sh
test -f tests/test_source_layout.sh
test -f include/yvex/decode.h
test -f include/yvex/logits.h
test -f include/yvex/sampling.h
test -f include/yvex/accounts.h

c_count="$(find tests -maxdepth 1 -type f \( -name 'test.c' -o -name 'test_*.c' \) | wc -l | tr -d ' ')"
if [ "$c_count" -gt 2 ]; then
    echo "too many top-level test C runners: $c_count"
    find tests -maxdepth 1 -type f -name 'test_*.c' | sort
    exit 1
fi

cli_count="$(find tests -maxdepth 1 -type f -name 'test_cli*.sh' | wc -l | tr -d ' ')"
if [ "$cli_count" -ne 0 ]; then
    echo "old CLI test scripts remain"
    find tests -maxdepth 1 -type f -name 'test_cli*.sh' | sort
    exit 1
fi

test -d tests/unit
test -d tests/unit/cuda
test -d src/cli/commands
test -d src/cli/input
test -d src/cli/render
test -d src/cli/io
test -d src/io
test -d src/cli/catalog
test -d src/cli/schema

test -f src/source/yvex_source_private.h
test -f src/source/yvex_source_manifest.c
test -f src/source/yvex_source_manifest.h
test -f src/source/yvex_source_scan.c
test -f src/source/yvex_source_scan.h
test -f src/source/yvex_native_weights.c
test -f src/source/yvex_native_weights.h
test -f src/source/yvex_safetensors_header.c
test -f src/source/yvex_safetensors_header.h
test -f src/source/yvex_source_report.c
test -f src/source/yvex_source_report.h
test -f src/source/yvex_source_write.c
test -f src/source/yvex_source_write.h
test -f src/cli/input/yvex_source_args.c
test -f src/cli/input/yvex_source_args.h
test -f src/cli/render/yvex_source_render.c
test -f src/cli/render/yvex_source_render.h
test -f src/io/yvex_json_writer.c
test -f src/io/yvex_json_writer.h
test -f src/generation/yvex_generation_report.c
test -f src/generation/yvex_generation_report.h
test -f src/generation/yvex_generation_trace.c
test -f src/generation/yvex_generation_trace.h
test -f src/generation/yvex_generation_private.h
test -f src/cli/input/yvex_generate_args.c
test -f src/cli/input/yvex_generate_args.h
test -f src/cli/render/yvex_generate_render.c
test -f src/cli/render/yvex_generate_render.h
test -f src/cli/render/yvex_generate_trace_render.c
test -f src/cli/render/yvex_generate_trace_render.h
test -f src/generation/yvex_kv_report.c
test -f src/generation/yvex_kv_report.h
test -f src/generation/yvex_kv_private.h
test -f src/cli/input/yvex_kv_args.c
test -f src/cli/input/yvex_kv_args.h
test -f src/cli/commands/yvex_kv_cli.c
test -f src/cli/render/yvex_kv_render.c
test -f src/cli/render/yvex_kv_render.h
test -f src/cli/catalog/kv_options.def
test -f src/cli/catalog/kv_fields.def
test -f src/cli/catalog/kv_boundaries.def

echo "test surface: ok"
