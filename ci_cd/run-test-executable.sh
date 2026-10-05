#!/usr/bin/env sh

set -eu

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

staging_dir="$1"
mem_analysis_tool="$2" # Either "valgrind", "leaks" or "none"
subdir="$(dirname "$3")"
exe="$(basename "$3")"

# Discard named arguments so we end up with parameters to the test executable
shift 3

printGreen() {
  IFS=' ' printf "\e[32m%s\e[0m\n" "$*"
}

printBoldGreen() {
  IFS=' ' printf "\e[1;32m%s\e[0m\n" "$*"
}

run_cmd() {
  printBoldGreen "Running test: $exe"
  printGreen "Memory analysis tool: $mem_analysis_tool"
  printGreen "Arguments: $*"
  "$@" && true
  ret=$?
  if [ "$ret" -ne 0 ] ; then
    printGreen "$exe failed with exit code $ret"
    return "$ret"
  fi
}

case "$mem_analysis_tool" in
  valgrind)
    run_cmd "$SCRIPTPATH/../scripts/run-valgrind.sh" "./$staging_dir/$subdir/$exe" "$SCRIPTPATH/../$subdir/tests/valgrind.supp" "$@"
    ;;
  leaks)
    # `leaks --atExit` exits with a status that reflects whether memory leaks were
    # found, and does not reliably propagate the test executable's own exit code:
    # it has been observed to report "0 leaks" and exit 0 even when the test itself
    # failed (e.g. when leaks cannot fully inspect the target process), which would
    # silently hide failing tests. So first run the executable directly to obtain
    # the authoritative pass/fail result, then run it under leaks to check for memory leaks.
    run_cmd ./"$staging_dir/$subdir/$exe" "$@"
    run_cmd leaks --atExit -- ./"$staging_dir/$subdir/$exe" "$@"
    ;;
  none)
    run_cmd "./$staging_dir/$subdir/$exe" "$@"
    ;;
  *)
    >&2 echo "Invalid value \"$mem_analysis_tool\" specified as 1st parameter. Please specify either \"valgrind\", \"leaks\", or \"none\"."
    exit 1
    ;;
esac
