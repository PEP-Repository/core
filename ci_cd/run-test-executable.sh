#!/usr/bin/env sh

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

staging_dir="$1"
valgrind="$2" # "valgrind" or "no-valgrind"
subdir="$(dirname "$3")"
exe="$(basename "$3")"

# Discard named arguments so we end up with parameters to the test executable
shift 3

run_cmd() {
  echo "$@"
  "$@"
}

run_cmd "./$staging_dir/$subdir/$exe" "$@"

case "$valgrind" in
  valgrind)
    "$SCRIPTPATH/../scripts/run-valgrind.sh" "./$staging_dir/$subdir/$exe" "$SCRIPTPATH/../$subdir/tests/valgrind.supp" "$@" || true
    ;;
  no-valgrind)
    >&2 echo "Skipping valgrind as specified"
    ;;
  *)
    >&2 echo "Invalid value \"$valgrind\" specified as 1st parameter. Please specify either \"valgrind\" or \"no-valgrind\"."
    exit 1
    ;;
esac
