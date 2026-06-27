#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
YVEXD_BIN=${YVEXD_BIN:-./yvexd}
OUT_ROOT=${YVEX_TEST_OUT_DIR:-build/tests/cli}

run_section() {
    name=$1
    script=$2

    printf 'cli: %s\n' "$name"
    YVEX_BIN="$YVEX_BIN" \
    YVEXD_BIN="$YVEXD_BIN" \
    YVEX_TEST_OUT_DIR="$OUT_ROOT/$name" \
        sh "$script"
}

if [ "${1:-}" = "--cuda" ]; then
    run_section cuda tests/cli/cuda.sh
    exit 0
fi

run_section core tests/cli/core.sh
run_section run tests/cli/run.sh
run_section chat tests/cli/chat.sh
run_section metrics tests/cli/metrics.sh
run_section server tests/cli/server.sh
run_section artifact-integrity tests/cli/artifact_integrity.sh
run_section artifact-corruption tests/cli/artifact_corruption.sh
run_section artifact-identity tests/cli/artifact_identity.sh
run_section artifact-metadata tests/cli/artifact_metadata.sh
run_section materialize tests/cli/materialize.sh
run_section materialize-gate tests/cli/materialize_gate.sh
run_section materialization-integrity-gate tests/cli/materialization_integrity_gate.sh
run_section fixture-graph tests/cli/fixture_graph.sh
run_section partial-graph tests/cli/partial_graph.sh
run_section source-manifest tests/cli/source_manifest.sh
run_section native-weights tests/cli/native_weights.sh
run_section gguf-template tests/cli/gguf_template.sh
run_section gguf-emit tests/cli/gguf_emit.sh
run_section tensor-map tests/cli/tensor_map.sh
run_section convert tests/cli/convert.sh
run_section model-gate tests/cli/model_gate.sh
run_section models tests/cli/models.sh
run_section model-aliases tests/cli/model_aliases.sh
run_section quant-job tests/cli/quant_job.sh
run_section quant-policy tests/cli/quant_policy.sh
run_section imatrix tests/cli/imatrix.sh

printf 'cli smoke: ok\n'
