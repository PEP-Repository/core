#!/usr/bin/env sh

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

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
    printGreen "Failed with exit code $ret"
    return "$ret"
  fi
}

case "$mem_analysis_tool" in
  valgrind)
    run_cmd "$SCRIPTPATH/../scripts/run-valgrind.sh" "./$staging_dir/$subdir/$exe" "$SCRIPTPATH/../$subdir/tests/valgrind.supp" "$@"
    ;;
  leaks)
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
