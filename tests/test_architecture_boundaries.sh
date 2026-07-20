#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
python3 tests/c_structure.py check architecture

if rg -n '#include[[:space:]]+"src/graph/private\.h"' src/cli; then
    echo "architecture: CLI depends on graph-private ABI" >&2
    exit 1
fi

printf '%s\n' '#include <yvex/internal/graph.h>' 'int main(void) { return 0; }' |
    "${CC:-cc}" -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L \
        -Iinclude -I. -std=c11 -Wall -Wextra -pedantic -Werror \
        -x c -fsyntax-only -

fail() {
    printf 'architecture: %s\n' "$1" >&2
    exit 1
}

for reachability_contract in \
    '### Executable reachability' \
    'production API directly' \
    'operator_command_available' \
    'cli_applicability=not_applicable'
do
    rg -F "$reachability_contract" AGENTS.md >/dev/null ||
        fail "executable-reachability contract is incomplete: $reachability_contract"
done

pending_identity_pattern='pending-payload-'\
'(plan|byte)-identity'
if rg -n "$pending_identity_pattern" src include; then
    fail "selected artifact admission retains a placeholder payload identity"
fi

recursive_cleanup_pattern='(^|[;&|()[:space:]])(command[[:space:]]+)?r'\
'm[[:space:]]+([^#;]*[[:space:]])?(-[[:alpha:]]*[rR][[:alpha:]]*|--recursive)([[:space:]]|$)'
for unsafe_flags in '-rf unsafe' '-fr unsafe' '-f -r unsafe' '--recursive unsafe'; do
    printf '%s%s\n' 'rm ' "$unsafe_flags" | rg "$recursive_cleanup_pattern" >/dev/null ||
        fail "destructive-cleanup guard misses recursive flags: $unsafe_flags"
done
if printf '%s%s\n' 'rm ' '-f owned-file' | rg "$recursive_cleanup_pattern" >/dev/null; then
    fail "destructive-cleanup guard rejects non-recursive file removal"
fi

maintained_scripts=$(
    {
        git ls-files '*.sh'
        git ls-files --others --exclude-standard '*.sh'
    } | LC_ALL=C sort -u
)
[ -n "$maintained_scripts" ] || fail "maintained-script inventory is empty"
while IFS= read -r script; do
    [ -f "$script" ] || fail "maintained-script scan input is missing: $script"
    if rg -n "$recursive_cleanup_pattern" "$script"; then
        fail "maintained script contains recursive rm cleanup: $script"
    fi
done <<EOF
$maintained_scripts
EOF

# Exercise cleanup ownership without exposing a real repository, model, or
# external path to deletion. Dangerous path checks use the validation-only API.
. tests/support/cleanup.sh
cleanup_probe=$(mktemp -d "${TMPDIR:-/tmp}/yvex-cleanup-guard.XXXXXX")
cleanup_external=$(mktemp -d "${TMPDIR:-/tmp}/cleanup-external.XXXXXX")
printf 'preserve\n' >"$cleanup_external/preserve"
mkdir -p "$cleanup_probe/nested"
printf 'remove\n' >"$cleanup_probe/nested/file"
ln -s "$cleanup_external" "$cleanup_probe/external-link"
yvex_test_cleanup "$cleanup_probe"
[ ! -e "$cleanup_probe" ] || fail "owned cleanup root survived deletion"
[ -f "$cleanup_external/preserve" ] || fail "cleanup followed a descendant symlink"
yvex_test_cleanup "$cleanup_probe" || fail "owned cleanup is not idempotent"

cleanup_link="${TMPDIR:-/tmp}/yvex-cleanup-link.$$"
ln -s "$cleanup_external" "$cleanup_link"
if yvex_test_cleanup_validate "$cleanup_link" >/dev/null 2>&1; then
    fail "cleanup admits a symlink root"
fi
unlink "$cleanup_link"

for refused_path in '' / "$PWD" "$HOME/lab/models" "$cleanup_external" \
    'build/tests/owned/../../..'; do
    if yvex_test_cleanup_validate "$refused_path" >/dev/null 2>&1; then
        fail "cleanup admits an unsafe path: ${refused_path:-<empty>}"
    fi
done

status_probe="${TMPDIR:-/tmp}/yvex-cleanup-status.$$"
set +e
yvex_test_cleanup_preserving_status 37 "$status_probe"
preserved_status=$?
yvex_test_cleanup_preserving_status 0 "$cleanup_external" >/dev/null 2>&1
cleanup_failure_status=$?
set -e
[ "$preserved_status" -eq 37 ] || fail "cleanup disguised the original test status"
[ "$cleanup_failure_status" -ne 0 ] || fail "cleanup failure was reported as success"
find "$cleanup_external" -xdev -depth -mindepth 1 -delete
rmdir "$cleanup_external"

if rg -n 'tests/reference/deepseek_attention|yvex_test_attention_reference_' \
    src include; then
    fail "production source references the test-only attention oracle"
fi

# The oracle may inspect immutable plans and decode admitted inputs. It may not
# call production equation, state-transition, selection, or reduction owners.
oracle_source=tests/reference/deepseek_attention.c
if rg -n \
    '(cpu_chunk_execute|cuda_token_execute|rolling_state_step_cpu)|yvex_attention_(activation_apply|compute_round|csa_select|hadamard_cpu|history_validate|output_project|reduce_chunk|rms_norm|rope_apply|topk_select|unit_rms_norm)[[:space:]]*\(' \
    "$oracle_source"; then
    fail "attention oracle calls a production numeric or composition owner"
fi

for product in "${YVEX_LIB:-build/lib/libyvex.a}" "${YVEX_BIN:-./yvex}"; do
    [ -f "$product" ] || fail "required production product is missing: $product"
    if nm -A "$product" | rg 'yvex_test_attention_reference_'; then
        fail "production product links the test-only attention oracle: $product"
    fi
done

# Link-time dependency admission keeps the independent oracle limited to
# immutable family facts, payload reads, descriptor lookup, and digest helpers.
reference_objects=${YVEX_REFERENCE_OBJS:-build/obj/tests/reference/deepseek_attention.o}
for object in $reference_objects; do
    [ -f "$object" ] || fail "attention oracle object is missing: $object"
    unexpected=$(
        nm -u "$object" | awk '{ print $NF }' |
        while IFS= read -r symbol; do
            case "$symbol" in
                yvex_error_clear|yvex_graph_lower_deepseek_v4|\
                yvex_materialization_session_read|yvex_model_register_deepseek_v4|\
                yvex_runtime_descriptor_find_role|yvex_sha256_*)
                    ;;
                yvex_*)
                    printf '%s\n' "$symbol"
                    ;;
            esac
        done
    )
    if [ -n "$unexpected" ]; then
        printf '%s\n' "$unexpected" >&2
        fail "attention oracle gained a production algorithm dependency: $object"
    fi
done
