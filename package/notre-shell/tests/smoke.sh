#!/bin/sh
set -eu

src_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../src" && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -f "$tmp_dir/notre-shell" "$tmp_dir/output.txt"; rmdir "$tmp_dir"' EXIT HUP INT TERM

${CC:-cc} -std=c11 -Wall -Wextra -Werror -O2 \
    "$src_dir/main.c" -o "$tmp_dir/notre-shell"
"$tmp_dir/notre-shell" >"$tmp_dir/output.txt" <<'INPUT'
version
echo 'alpha beta'
pwd
exit 0
INPUT
grep -Fq 'NotreShell 0.0.1' "$tmp_dir/output.txt"
grep -Fq 'alpha beta' "$tmp_dir/output.txt"
grep -Fq '/' "$tmp_dir/output.txt"
printf '%s\n' 'NotreShell smoke test passed.'
