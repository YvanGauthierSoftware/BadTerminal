#!/bin/sh
set -eu

terminal=${1:?usage: test_cli.sh PATH_TO_BADTERM}
transcript=$(mktemp "${TMPDIR:-/tmp}/badterm-test.XXXXXX")
trap 'rm -f "$transcript"' EXIT

set +e
script -q "$transcript" "$terminal" /bin/sh -c 'printf CLI_TEST; exit 19' >/dev/null
result=$?
set -e

output=$(tr -d '\r' < "$transcript")
case "$output" in
    *CLI_TEST*) ;;
    *)
        printf '%s\n' "FAIL: CLI output did not contain CLI_TEST" >&2
        exit 1
        ;;
esac

if [ "$result" -ne 19 ]; then
    printf 'FAIL: CLI exit status was %s, expected 19\n' "$result" >&2
    exit 1
fi

printf '%s\n' 'CLI test: ok'