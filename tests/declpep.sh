#!/usr/bin/env bash
# shellcheck disable=SC2016 # Reason: we are intentionally using single quoted strings to avoid expansion

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 {cleanup|setup} <json>"
  exit 1
fi

readonly command="$1"
readonly json="$2"

# Runs a jq filter on $json_file and then feeds this into a jq format string
# The first argument specifies the filter
# The other arguments are concatenated into a single format string
jq_wrapper() {
  local -r NAME_REGEX='[a-zA-Z_][a-zA-Z0-9_]*'

  # Treat missing or null array fields as empty by replacing each array value iterator with a defaulted version.
  missing_arrays_as_empty() {
    local -r target="\.($NAME_REGEX)\[]"     # .<FIELD_NAME>[]
    local -r replacement='(.\1 \/\/ \[])\[]' # (.<FIELD_NAME> // [])[]
    sed -E "s/$target/$replacement/g"
  }

  # Treat missing or null map fields as empty by replacing each map value iterator with a defaulted version.
  missing_maps_as_empty() {
    local -r target="\.($NAME_REGEX) \| to_entries\[]" # .<FIELD_NAME> | to_entries[]
    local -r replacement='.\1 \/\/ {} | to_entries\[]' # .<FIELD_NAME> // {} | to_entries[]
    sed -E "s/$target/$replacement/g"
  }

  local -r filter=$(echo "$1" | missing_arrays_as_empty | missing_maps_as_empty)
  shift
  local -r genlines=$(printf "%s" "${@//\"/\\\"}")

  echo "$json" | jq -r "$filter | \"$genlines\""
}

# Move lines with the substring before those without it
partition_by_substring() {
  grep_nofail() { grep "$@" || cat; } # does not fail if there is no match
  local -r substring="$1"
  local -r tempfile=$(mktemp) && trap 'rm -f "$tempfile"' RETURN

  tee "$tempfile" | grep_nofail -e "$substring"
  cat "$tempfile" | grep_nofail -v "$substring"
}

generate_pep_commands_in_setup_order() {
  pep_as() { echo "pepcli --oauth-token-group '$1'"; }
  local -r AS_ACCESS_ADMIN=$(pep_as 'Access Administrator')
  local -r AS_DATA_ADMIN=$(pep_as 'Data Administrator')

  local -r createOrRemove=$([ "$command" == "setup" ] && echo "create" || echo "remove")
  local -r addOrRemove=$([ "$command" == "setup" ] && echo "addTo" || echo "removeFrom")

  # user groups
  jq_wrapper '.userGroups[]' \
    "$AS_ACCESS_ADMIN user group $createOrRemove \\(.name | @sh)"

  # individual users
  jq_wrapper '.userGroups[] | .name as $group | .users[]' \
    "$AS_ACCESS_ADMIN user $createOrRemove \\(. | @sh)\n" \
    "$AS_ACCESS_ADMIN user $addOrRemove \\(. | @sh) \\(\$group | @sh)" |
    partition_by_substring "$AS_ACCESS_ADMIN user $createOrRemove " # trailing space is significant

  # additional identifiers
  if [ "$command" == "setup" ]; then
    jq_wrapper '.userGroups[] | .additionalIdentifiers | to_entries[] | .key as $user | .value[]' \
      "$AS_ACCESS_ADMIN user addIdentifier \\(\$user | @sh) \\(. | @sh)"
  else
    jq_wrapper '.userGroups[] | .additionalIdentifiers | to_entries[] | .value[]' \
      "$AS_ACCESS_ADMIN user removeIdentifier \\(. | @sh)"
  fi

  # column groups
  jq_wrapper '.columnGroups[]' \
    "$AS_DATA_ADMIN ama columnGroup $createOrRemove \\(.name | @sh)"

  # column group access rules
  jq_wrapper '.columnGroups[] | .name as $cGroup | .cgars | to_entries[]' \
    "$AS_ACCESS_ADMIN ama cgar $createOrRemove \\(\$cGroup | @sh) \\(.key | @sh) \\(.value[] | @sh)"

  # individual columns
  jq_wrapper '.columnGroups[] | .name as $group | .columns[]' \
    "$AS_DATA_ADMIN ama column $createOrRemove \\(. | @sh)\n" \
    "$AS_DATA_ADMIN ama column $addOrRemove \\(. | @sh) \\(\$group | @sh)" |
    partition_by_substring "$AS_DATA_ADMIN ama column $createOrRemove " # trailing space is significant

  # subject groups
  jq_wrapper '.subjectGroups[]' \
    "$AS_DATA_ADMIN ama group $createOrRemove \\(.name | @sh)"

  # subject group access rules
  jq_wrapper '.subjectGroups[] | .name as $cGroup | .pgars | to_entries[]' \
    "$AS_ACCESS_ADMIN ama pgar $createOrRemove \\(\$cGroup | @sh) \\(.key | @sh) \\(.value[] | @sh)"

  # individual subjects
  if [ "$command" == "setup" ]; then
    jq_wrapper '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
      '\($bash_var)="$(' \
      "$AS_DATA_ADMIN register id | grep 'identifier:' | cut -d ':' -f2 | tr -d '[:space:]'" \
      ')"'
  fi

  jq_wrapper '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
    "$AS_DATA_ADMIN ama group $addOrRemove \\(\$group | @sh) \${\\(\$bash_var)}"

  if [ "$command" == "setup" ]; then
    jq_wrapper '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var | .value | to_entries[]' \
      "$AS_DATA_ADMIN store -p \${\\(\$bash_var)} -c \\(.key | @sh) -d \\(.value | @sh)"
  fi
}

if [[ "$1" == "setup" ]]; then
  generate_pep_commands_in_setup_order
elif [[ "$1" == "cleanup" ]]; then
  generate_pep_commands_in_setup_order | tac
else
  echo "\"$command\" is not a supported command."
  exit 1
fi
