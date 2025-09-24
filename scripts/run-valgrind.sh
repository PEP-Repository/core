#!/usr/bin/env sh

set -o errexit
set -o nounset

exe="$1"
supp="$2"
# Discard named arguments so we end up with parameters to the test executable
shift
shift

run_cmd() {
  echo "$@"
  "$@"
}

# Callers provide a path to where a suppressions file _may_ be located.
# Only pass it to valgrind if the file exists.
supp_clause=
if [ -f "$supp" ]; then
  supp_clause="--suppressions=$supp"
fi

run_cmd valgrind --error-exitcode=1 --track-origins=yes --track-fds=yes --leak-check=full --gen-suppressions=all $supp_clause -v "$exe" "$@"
