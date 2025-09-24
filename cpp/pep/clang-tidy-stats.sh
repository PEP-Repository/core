#!/usr/bin/env sh
set -eu

profile_dir="${1:?Pass path/to/clang-tidy-profile/ dir}"

jq --slurp '
  [
    [
      .[]
      | .file as $file
      | .profile | to_entries[] + {file: $file}
      | select(.key | test("\\.wall$"))
    ] | group_by(.key)[]
    | {key: .[0].key, value: {
      total: [.[].value] | add,
      median: sort_by(.value)[length / 2 | floor].value,
      max: [sort_by(.value) | reverse[:5][] | {value, file}]
    }}
  ] | sort_by(.value.total)
  | from_entries' \
  -- "$profile_dir"/*.json

>&2 echo 'Durations in seconds, largest total last'

# User should delete obsolete files themselves
find "$profile_dir" -name '*.json' -printf '%T+ %p\n' | sort |
  (printf 'Oldest file: '; head -1; printf 'Newest file: '; tail -1) >&2
