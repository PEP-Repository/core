#!/usr/bin/env bash

#
# When running an app bundle on MacOS, we don't get to see stdout and stderr.
# With this script we can run a bundle, and still see stdout and stderr
#
# Usage:
#    runWithRedirectedOutput.sh [bundle="pepAssessor.app"] [executable-name="pepAssessor"] [stdout-file="$TMPDIR/stdout"] [stderr-file="$TMPDIR/stderr"]
#
# Set the $LLDB environment variable to the location of the lldb-binary, if you do not want to use the Xcode-version.
#

make_absolute() {
  echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}
SCRIPT_DIR="$(make_absolute "$(dirname "$0")")"
readonly SCRIPT_DIR

bundle="$(make_absolute "${1:-"pepAssessor.app"}")"
readonly bundle
readonly executable=${2:-"pepAssessor"}
stdout="$(make_absolute "${3:-"/tmp/stdout"}")"
readonly stdout
stderr="$(make_absolute "${4:-"/tmp/stderr"}")"
readonly stderr

touch "$stdout" "$stderr"

"$SCRIPT_DIR/redirectOutput.sh" "$stdout" "$stderr" "$executable" &

sleep 1

open "$bundle"

tail -F "$stdout" "$stderr"
