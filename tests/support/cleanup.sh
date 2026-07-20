#!/bin/sh

# Reports whether any component already present in an absolute path is a
# symlink. Missing suffixes are safe to inspect because cleanup never creates
# them while validating ownership.
_yvex_test_path_has_symlink() {
    _yvex_cursor=
    _yvex_remainder=${1#/}

    while test -n "$_yvex_remainder"; do
        case "$_yvex_remainder" in
            */*)
                _yvex_component=${_yvex_remainder%%/*}
                _yvex_remainder=${_yvex_remainder#*/}
                ;;
            *)
                _yvex_component=$_yvex_remainder
                _yvex_remainder=
                ;;
        esac
        _yvex_cursor=$_yvex_cursor/$_yvex_component
        test ! -L "$_yvex_cursor" || return 0
    done
    return 1
}

# Validates one path without deleting it. Test-owned resources are restricted
# to descendants of repository build/tests or a yvex-* root directly beneath
# the configured temporary directory.
yvex_test_cleanup_validate() {
    test "$#" -eq 1 || {
        printf 'test cleanup: exactly one path is required for validation\n' >&2
        return 1
    }

    _yvex_path=$1
    test -n "$_yvex_path" || {
        printf 'test cleanup: empty path refused\n' >&2
        return 1
    }
    case "$_yvex_path" in
        /|.|..|*//*|*/./*|*/../*|*/.|*/..)
            printf 'test cleanup: non-canonical path refused: %s\n' "$_yvex_path" >&2
            return 1
            ;;
    esac

    _yvex_repo_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
        printf 'test cleanup: repository root is unavailable\n' >&2
        return 1
    }
    _yvex_repo_root=$(cd "$_yvex_repo_root" && pwd -P) || return
    _yvex_cwd=$(pwd -P) || return
    test "$_yvex_cwd" = "$_yvex_repo_root" || {
        printf 'test cleanup: tests must run from the repository root\n' >&2
        return 1
    }

    case "$_yvex_path" in
        /*) _yvex_absolute=$_yvex_path ;;
        *) _yvex_absolute=$_yvex_repo_root/$_yvex_path ;;
    esac

    _yvex_tmp_root=${TMPDIR:-/tmp}
    case "$_yvex_tmp_root" in
        /*) ;;
        *)
            printf 'test cleanup: temporary root must be absolute: %s\n' "$_yvex_tmp_root" >&2
            return 1
            ;;
    esac
    test -d "$_yvex_tmp_root" || {
        printf 'test cleanup: temporary root is unavailable: %s\n' "$_yvex_tmp_root" >&2
        return 1
    }
    if _yvex_test_path_has_symlink "$_yvex_tmp_root"; then
        printf 'test cleanup: symlink temporary root refused: %s\n' "$_yvex_tmp_root" >&2
        return 1
    fi
    _yvex_tmp_root=$(cd "$_yvex_tmp_root" && pwd -P) || return

    case "$_yvex_absolute" in
        "$_yvex_repo_root"/build/tests/*) ;;
        "$_yvex_tmp_root"/yvex-?*) ;;
        *)
            printf 'test cleanup: path outside an owned test root refused: %s\n' \
                "$_yvex_path" >&2
            return 1
            ;;
    esac
    if _yvex_test_path_has_symlink "$_yvex_absolute"; then
        printf 'test cleanup: symlink path component refused: %s\n' "$_yvex_path" >&2
        return 1
    fi
}

# Deletes only caller-named test resources after validating every argument.
# The two-pass protocol prevents an invalid later path from causing a partial
# cleanup of earlier paths.
yvex_test_cleanup() {
    test "$#" -gt 0 || {
        printf 'test cleanup: at least one path is required\n' >&2
        return 1
    }

    for _yvex_path do
        yvex_test_cleanup_validate "$_yvex_path" || return
    done
    for _yvex_path do
        test -e "$_yvex_path" || continue
        if test -d "$_yvex_path"; then
            find "$_yvex_path" -xdev -depth -mindepth 1 -delete || return
            rmdir "$_yvex_path" || return
        else
            rm -f -- "$_yvex_path" || return
        fi
    done
}

# Combines cleanup with a previously captured command status. Cleanup failure
# is observable after success but never disguises an earlier test failure.
yvex_test_cleanup_preserving_status() {
    test "$#" -gt 1 || {
        printf 'test cleanup: status and at least one path are required\n' >&2
        return 1
    }
    _yvex_original_status=$1
    shift
    case "$_yvex_original_status" in
        ''|*[!0-9]*)
            printf 'test cleanup: invalid original status: %s\n' \
                "$_yvex_original_status" >&2
            return 1
            ;;
    esac
    test "$_yvex_original_status" -le 255 || {
        printf 'test cleanup: original status is out of range: %s\n' \
            "$_yvex_original_status" >&2
        return 1
    }

    if yvex_test_cleanup "$@"; then
        return "$_yvex_original_status"
    else
        _yvex_cleanup_status=$?
    fi
    test "$_yvex_original_status" -eq 0 || return "$_yvex_original_status"
    return "$_yvex_cleanup_status"
}
