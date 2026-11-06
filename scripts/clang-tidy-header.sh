#!/usr/bin/env sh
# Runs clang-tidy on a header file, making it the main file so that misc-include-cleaner checks its #includes.
#
# Usage: clang-tidy-header.sh <compile_commands.json> <target> <header> <clang-tidy> [<args>...]
#
# The header is analyzed with the compile command of a source file of <target> (the target that the header belongs to),
# instead of letting clang-tidy guess a command from a (possibly unrelated) source with a similar path.
# Fails if <target> has no compile command.
set -eu

database="$1"
target="$2"
header="$3"
shift 3

directory="$(mktemp -d)"
trap 'rm -rf "$directory"' EXIT

# Create a compilation database with an entry for the header, based on (the first) source file of the target
jq --arg marker "/CMakeFiles/$target.dir/" --arg header "$header" '
  [first(.[] | select(has("command") and ((.output // "") | contains($marker))))
    | . as $entry
    | {directory: $entry.directory, file: $header, command: ($entry.command | split($entry.file) | join($header))}]
' "$database" > "$directory/compile_commands.json"
if [ "$(jq length "$directory/compile_commands.json")" -eq 0 ]; then
  # E.g. a header-only target. Don't let clang-tidy guess a command from a (possibly unrelated) source
  echo "$0: no compile command found for target $target (needed to check $header)" >&2
  exit 1
fi

"$@" -p "$directory" "$header"
