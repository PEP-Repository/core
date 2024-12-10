#!/usr/bin/env bash
set -e

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

testExe=$1
testExeDir=$(dirname "$0")

listOfTests="$($testExe --gtest_list_tests)"

currentTestCase=
while IFS= read -r line; do
  if [[ "$line" == *. ]]
  then
    currentTestCase=$line
  else
    if [[ "$line" == '  '* ]]
    then
      trimmed="$(tr -d '[:space:]' <<< "$line")"
      test=${currentTestCase}${trimmed}
      echo "Running valgrind for test $test"
          "$SCRIPTPATH/run-valgrind.sh" "$testExe" "${testExeDir}/tests/valgrind.supp" "--gtest_filter=$test"
    fi
  fi
done <<< "$listOfTests"
