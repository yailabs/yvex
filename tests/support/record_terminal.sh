#!/bin/sh
# Preserve the real terminal, flush and child-status contract across native script variants.
set -eu
if test "$(uname -s)" != Darwin; then
    exec script "$@"
fi
command=
while test "$#" -gt 0; do
    case "$1" in
        -q|-f|-e) shift ;;
        -c) test "$#" -ge 2; command=$2; shift 2 ;;
        --) shift; break ;;
        -*) printf 'unsupported terminal recorder option: %s\n' "$1" >&2; exit 2 ;;
        *) break ;;
    esac
done
test -n "$command" && test "$#" -eq 1
exec python3 tests/support/record_terminal.py "$1" "$command"
