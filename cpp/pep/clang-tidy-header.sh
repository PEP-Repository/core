#!/usr/bin/env sh
# Runs clang-tidy on a header file, making it the main file so that misc-include-cleaner checks its #includes.
#
# Usage: clang-tidy-header.sh <compile_commands.json> <target> <header> <stamp> <clang-tidy> [<args>...]
#
# The header is analyzed with the compile command of a source file of <target> (the target that the header belongs to),
# instead of letting clang-tidy guess a command from a (possibly unrelated) source with a similar path.
# On success, writes a dependency file <stamp>.d (listing the headers that the header includes) and touches <stamp>.
set -eu

database="$1"
target="$2"
header="$3"
stamp="$4"
shift 4

# Don't leave a stamp from an earlier (successful) check behind if this check fails
rm -f "$stamp"

directory="$(mktemp -d)"
trap 'rm -rf "$directory"' EXIT

# Create a compilation database with an entry for the header, based on (the first) source file of the target
jq --arg marker "/CMakeFiles/$target.dir/" --arg header "$header" '
  [first(.[] | select(has("command") and ((.output // "") | contains($marker))))
    | . as $entry
    | {directory: $entry.directory, file: $header, command: ($entry.command | split($entry.file) | join($header))}]
' "$database" > "$directory/compile_commands.json"
if [ "$(jq length "$directory/compile_commands.json")" -ne 0 ]; then
  database_dir="$directory"
else # E.g. a header-only target: let clang-tidy pick the command of a similar source
  database_dir="$(dirname "$database")"
fi

# Have the preprocessor (-Wp,...) write a dependency file listing the (non-system) headers that the header
# includes (-MMD,<file>). This must be a single argument: clang-tidy removes arguments that start with -M.
"$@" -p "$database_dir" "--extra-arg=-Wp,-MMD,$stamp.d.tmp" "$header"

# The dependency file names a default target (e.g. Header.o): make it name the stamp file instead
{
  printf '%s:' "$(printf '%s' "$stamp" | sed 's/ /\\ /g')"
  sed '1s/^[^:]*://' "$stamp.d.tmp"
} > "$stamp.d"
rm "$stamp.d.tmp"
touch "$stamp"
