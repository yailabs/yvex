#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

fail() {
    printf 'repository-layout: %s\n' "$1" >&2
    exit 1
}

# The canonical clean target may remove repository build products. No other
# Make recipe may use an unbounded recursive deletion as test cleanup.
make_cleanup=$(awk '
    /^[^[:space:]#][^=]*:/ { target = $0 }
    /^\t/ && /rm[[:space:]]+([^[:space:]]+[[:space:]]+)*(--recursive|-[^[:space:]]*[rR][^[:space:]]*)/ && target !~ /^clean[[:space:]]*:/ {
        print FNR ":" target ":" $0
    }
' Makefile)
if [ -n "$make_cleanup" ]; then
    printf '%s\n' "$make_cleanup" >&2
    fail "recursive cleanup outside the canonical clean target"
fi

# Operator attention and its sanitizer wrappers own mkdtemp roots and must
# never grow broad deletion shortcuts as the command surface evolves.
if rg -n --glob '*attention*.sh' \
    'rm[[:space:]]+([^[:space:]]+[[:space:]]+)*(--recursive|-[^[:space:]]*[rR][^[:space:]]*)' tests; then
    fail "attention operator or sanitizer script uses recursive rm"
fi

python3 tests/c_structure.py check layout
