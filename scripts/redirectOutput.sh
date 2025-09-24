#!/usr/bin/env bash

#
# When running an app bundle on MacOS, we don't get to see stdout and stderr.
# With this script we can redirect stdout and stderr to a file.
#
# Usage:
#    redirectOutput.sh [stdout-file="stdout"] [stderr-file="stderr"] [executable-name="pepAssessor"]
#
# Then open the app bundle, it will wait for the specified executable to be started
#
# Set the $LLDB environment variable to the location of the lldb-binary, if you do not want to use the Xcode-version.
#

make_absolute() {
  echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

stdout="$(make_absolute "${1:-"stdout"}")"
readonly stdout
stderr="$(make_absolute "${2:-"stderr"}")"
readonly stderr
readonly executable=${3:-"pepAssessor"}

readonly lldb=${LLDB:-"/Applications/Xcode.app/Contents/Developer/usr/bin/lldb"}

"$lldb" <<EOF
process attach --name $executable --waitfor
expr (int) close(1)
expr (int) creat("$stdout", 0600)
expr (int) close(2)
expr (int) creat("$stderr", 0600)
continue
EOF