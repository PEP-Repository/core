#!/usr/bin/env bash

set -eu

function die() {
  exit 1
}

function invalid_invocation() {
  >&2 echo "Usage: $0 <language> <path-to-Messages.proto> <output-dir> <output-file-name>"
  >&2 echo "    where <language> is (c++|go)"
  die
}

if [ "$#" -ne 4 ]; then
  >&2 echo "Incorrect number of command line parameters."
  invalid_invocation
fi

# Convert CRLF to LF to ensure the same binary content is processed on all platforms
cp "$2" "$3"/Messages.proto
if file "$3"/Messages.proto | grep -q 'CRLF'; then
  dos2unix "$3"/Messages.proto
fi

# BSD/macOS provides "shasum" iso "sha512sum": see e.g. https://www.freebsd.org/cgi/man.cgi?query=shasum&sektion=1&manpath=freebsd-release-ports
sha512cmd=$(command -v sha512sum || echo "shasum --algorithm 512")

thesum=$($sha512cmd "$3"/Messages.proto | cut -d " " -f 1 | tr -d '[:space:]')
if [ -z "$thesum" ]
then
  >&2 echo "Could not calculate SHA-512 checksum for $2"
  die
fi

# Redirection to output file cannot be reliably done in CMake.
# See e.g. https://stackoverflow.com/a/31582345
outfile=$3/$4

case "$1" in
  c | c++ | cpp )
    echo \#pragma once > "$outfile"
    echo \#define MESSAGES_PROTO_CHECKSUM \""$thesum"\" >> "$outfile"
    ;;
  go )
    echo package pep_proto > "$outfile"
    echo const MESSAGES_PROTO_CHECKSUM = \""$thesum"\" >> "$outfile"
    ;;
  * )
    >&2 echo "Unsupported language: $1"
    invalid_invocation
    ;;
esac
