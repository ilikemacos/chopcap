#!/usr/bin/env bash
# Chopcap test suite.
#
#   ./tests/run_tests.sh            run everything
#   ./tests/run_tests.sh --bless    rewrite the .expected files from the run
#
# Each tests/programs/NAME.chop is run with stdout and stderr merged and
# compared against NAME.expected.  NAME.in, when present, is fed on stdin.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/chopcap"
PROGRAMS="$ROOT/tests/programs"
BLESS=0
[ "${1:-}" = "--bless" ] && BLESS=1

pass=0
fail=0
failed_names=()

green() { printf '\033[32m%s\033[0m' "$1"; }
red()   { printf '\033[31m%s\033[0m' "$1"; }

ok()   { pass=$((pass+1)); printf '  %s %s\n' "$(green ok)" "$1"; }
bad()  { fail=$((fail+1)); failed_names+=("$1"); printf '  %s %s\n' "$(red FAIL)" "$1"; }

if [ ! -x "$BIN" ]; then
    echo "chopcap binary not found at $BIN — run 'make' first." >&2
    exit 1
fi

# ---------------------------------------------------------------- unit tests
echo
echo "Unit tests (lexer, parser, values, environments)"
if [ -x "$ROOT/build/unit_tests" ]; then
    if out=$("$ROOT/build/unit_tests" 2>&1); then
        echo "$out" | sed 's/^/  /'
        pass=$((pass+1))
    else
        echo "$out" | sed 's/^/  /'
        bad "unit_tests"
    fi
else
    echo "  (skipped: build/unit_tests not built — run 'make test')"
fi

# ------------------------------------------------------------ program tests
echo
echo "Program tests"
for src in "$PROGRAMS"/*.chop; do
    name="$(basename "$src" .chop)"
    # helpers.chop is imported by another test, not run on its own.
    [ "$name" = "helpers" ] && continue
    expected="$PROGRAMS/$name.expected"
    stdin_file="$PROGRAMS/$name.in"

    if [ -f "$stdin_file" ]; then
        actual="$("$BIN" "$src" < "$stdin_file" 2>&1)"
    else
        actual="$("$BIN" "$src" < /dev/null 2>&1)"
    fi

    if [ "$BLESS" = "1" ]; then
        printf '%s\n' "$actual" > "$expected"
        ok "$name (blessed)"
        continue
    fi

    if [ ! -f "$expected" ]; then
        bad "$name (no .expected file)"
        continue
    fi

    if [ "$actual" = "$(cat "$expected")" ]; then
        ok "$name"
    else
        bad "$name"
        diff <(printf '%s\n' "$(cat "$expected")") <(printf '%s\n' "$actual") | sed 's/^/      /'
    fi
done

# ---------------------------------------------------------------- CLI tests
echo
echo "CLI tests"

check() { # name, expected-substring, actual
    if printf '%s' "$3" | grep -qF -- "$2"; then ok "$1"; else
        bad "$1"
        printf '      wanted to find: %s\n      got: %s\n' "$2" "$3"
    fi
}

check "chopcap --version"  "Chopcap 0.1.0"      "$("$BIN" --version)"
check "chopcap -v"         "Chopcap 0.1.0"      "$("$BIN" -v)"
check "chopcap version"    "Chopcap 0.1.0"      "$("$BIN" version)"
check "chopcap --help"     "Usage:"             "$("$BIN" --help)"
check "chopcap -h"         "chopcap run"        "$("$BIN" -h)"
check "chopcap -e"         "4"                  "$("$BIN" -e 'say 2 + 2')"
check "chopcap run FILE"   "Hello, world!"      "$("$BIN" run "$PROGRAMS/01_hello.chop")"
check "chopcap FILE"       "Hello, world!"      "$("$BIN" "$PROGRAMS/01_hello.chop")"
check "missing file"       "cannot open"        "$("$BIN" no_such_file.chop 2>&1)"
check "unknown option"     "not a known option" "$("$BIN" --wat 2>&1)"

# Exit codes
"$BIN" "$PROGRAMS/01_hello.chop" >/dev/null 2>&1
[ $? -eq 0 ] && ok "exit code 0 on success" || bad "exit code 0 on success"
"$BIN" "$PROGRAMS/17_err_undefined.chop" >/dev/null 2>&1
[ $? -ne 0 ] && ok "non-zero exit code on error" || bad "non-zero exit code on error"

# REPL
repl_out="$(printf 'say "hi"\n1 + 2\nx = 4\nx * 2\nfun sq(n):\n    return n * n\n\nsq(5)\nexit\n' | "$BIN" 2>&1)"
check "repl runs statements"  "hi"  "$repl_out"
check "repl echoes values"    "3"   "$repl_out"
check "repl keeps variables"  "8"   "$repl_out"
check "repl defines blocks"   "25"  "$repl_out"
repl_err="$(printf 'say nope\nsay "after"\nexit\n' | "$BIN" 2>&1)"
check "repl survives errors"  "after" "$repl_err"

# --------------------------------------------------------------- examples
echo
echo "Example programs"
for ex in "$ROOT"/examples/*.chop; do
    [ -f "$ex" ] || continue
    name="$(basename "$ex")"
    # Examples that ask questions get a canned set of answers.
    input=$(printf 'Ada\n7\n50\n25\n12\n7\n3\nquit\n')
    if out=$(printf '%s' "$input" | "$BIN" "$ex" 2>&1); then
        ok "examples/$name"
    else
        bad "examples/$name"
        printf '%s\n' "$out" | sed 's/^/      /'
    fi
done

# ---------------------------------------------------------------- website -
echo
echo "Website"
if command -v node >/dev/null 2>&1; then
    node "$ROOT/website/build.js" >/dev/null 2>&1 || bad "website build"
    if out=$(node "$ROOT/website/check-links.js" 2>&1); then
        printf '%s\n' "$out" | sed 's/^/  /'
        pass=$((pass+1))
    else
        printf '%s\n' "$out" | sed 's/^/  /'
        bad "website links"
    fi
    if out=$(node "$ROOT/website/check-examples.js" 2>&1); then
        printf '%s\n' "$out" | sed 's/^/  /'
        pass=$((pass+1))
    else
        printf '%s\n' "$out" | sed 's/^/  /'
        bad "website examples"
    fi
else
    echo "  (skipped: node is not installed)"
fi

# ------------------------------------------- website playground parity ----
echo
echo "Playground parity (the JavaScript build of Chopcap)"
if command -v node >/dev/null 2>&1; then
    if out=$(node "$ROOT/tests/js_parity.js" 2>&1); then
        printf '%s\n' "$out" | tail -2 | sed 's/^/  /'
        pass=$((pass+1))
    else
        printf '%s\n' "$out" | sed 's/^/  /'
        bad "js_parity"
    fi
else
    echo "  (skipped: node is not installed)"
fi

echo
if [ "$fail" -eq 0 ]; then
    printf '%s  %d passed\n\n' "$(green 'All tests passed.')" "$pass"
    exit 0
else
    printf '%s  %d passed, %d failed:\n' "$(red 'Tests failed.')" "$pass" "$fail"
    for n in "${failed_names[@]}"; do printf '  - %s\n' "$n"; done
    echo
    exit 1
fi
