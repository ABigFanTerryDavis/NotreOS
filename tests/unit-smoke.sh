#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -f "$TMP_DIR/notre-init" "$TMP_DIR/init-version.txt"; rmdir "$TMP_DIR"' EXIT HUP INT TERM

make -C "$ROOT_DIR/package/notre-shell" test
cc -std=c11 -Wall -Wextra -Werror -O2 \
    "$ROOT_DIR/package/notre-init/src/main.c" -o "$TMP_DIR/notre-init"
"$TMP_DIR/notre-init" --version >"$TMP_DIR/init-version.txt"
grep -Fqx 'NotreInit 0.0.1' "$TMP_DIR/init-version.txt"
printf '%s\n' 'NotreInit smoke test passed.'
