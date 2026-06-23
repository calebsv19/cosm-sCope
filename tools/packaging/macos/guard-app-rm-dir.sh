#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <path> <expected-app-name> [desktop]" >&2
    exit 2
fi

target_path="${1%/}"
expected_app_name="$2"
mode="${3:-package}"
home_dir="${HOME:-}"

fail() {
    echo "refusing unsafe app removal path: $1" >&2
    exit 1
}

[ -n "$target_path" ] || fail "empty path"
[ -n "$expected_app_name" ] || fail "empty app name"

case "$expected_app_name" in
    *.app) ;;
    *) fail "expected app name must end in .app" ;;
esac

case "$target_path" in
    /|.|..) fail "$target_path" ;;
    *"/../"*|../*|*/..|*"/./"*|./*|*/.) fail "$target_path" ;;
esac

if [ -n "$home_dir" ]; then
    home_dir="${home_dir%/}"
    [ "$target_path" != "$home_dir" ] || fail "$target_path"
    [ "$target_path" != "$home_dir/Desktop" ] || fail "$target_path"
fi

target_base="${target_path##*/}"
[ "$target_base" = "$expected_app_name" ] || fail "$target_path (expected basename $expected_app_name)"

case "$mode" in
    desktop)
        [ -n "$home_dir" ] || fail "HOME is unset"
        case "$target_path" in
            /*) ;;
            *) fail "$target_path (desktop path must be absolute)" ;;
        esac
        target_parent="${target_path%/*}"
        [ "$target_parent" = "$home_dir/Desktop" ] || fail "$target_path (desktop path must be under $home_dir/Desktop)"
        ;;
    package)
        ;;
    *)
        fail "unknown guard mode $mode"
        ;;
esac

exit 0
