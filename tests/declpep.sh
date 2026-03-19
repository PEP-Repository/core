#!/usr/bin/env bash
# shellcheck disable=SC2016 # Reason: we are intentionally using single quoted strings to avoid expansion

set -uo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 {cleanup|setup} <json>"
  exit 1
fi

command="$1"
json="$2"

# Runs a jq filter on $json_file and then feeds this into a jq format string
# The first argument specifies the filter
# The other arguments are concatenated into a single format string
jqr() {
  # Treat missing or null array fields as empty by replacing each array value iterator with a defaulted version.
  missing_arrays_as_empty(){
    local -r target='\.([a-zA-Z_][a-zA-Z0-9_]*)\[]' # .<FIELD_NAME>[]
    local -r replacement='(.\1 \/\/ \[])\[]'        # (.<FIELD_NAME> // [])[]
    sed -E "s/$target/$replacement/g"
  }

  # Treat missing or null map fields as empty by replacing each map value iterator with a defaulted version.
  missing_maps_as_empty() {
    local -r target='\.([a-zA-Z_][a-zA-Z0-9_]*) \| to_entries\[]' # .<FIELD_NAME> | to_entries[]
    local -r replacement='.\1 \/\/ {} | to_entries\[]'            # .<FIELD_NAME> // {} | to_entries[]
    sed -E "s/$target/$replacement/g"
  }

  local -r filter=$(echo "$1" | missing_arrays_as_empty | missing_maps_as_empty)
  shift
  local -r genlines=$(printf "%s" "${@//\"/\\\"}")

  echo "$json" | jq -r "$filter | \"$genlines\""
}

# Prints an empty line
empty_line() {
  echo
}

# Move lines with the substring before those without it
partition_by_substring() {
  local substring="$1"

  local tempfile
  tempfile=$(mktemp) && trap 'rm -f "$tempfile"' RETURN

  tee "$tempfile" | grep -e "$substring"
  cat "$tempfile" | grep -v "$substring"
}

generate_pep_commands_in_setup_order() {
  createOrRemove=$([ "$command" == "setup" ] && echo "create" || echo "remove")
  addOrRemove=$([ "$command" == "setup" ] && echo "addTo" || echo "removeFrom")

  # user groups
  jqr '.userGroups[]' \
    'pepcli --oauth-token-group "Access Administrator" user group '"$createOrRemove"' \(.name | @sh)'

  # individual users
  jqr '.userGroups[] | .name as $group | .users[]' \
    'pepcli --oauth-token-group "Access Administrator" user '"$createOrRemove"' \(. | @sh)' "\n" \
    'pepcli --oauth-token-group "Access Administrator" user '"$addOrRemove"' \(. | @sh) \($group | @sh)' |
    partition_by_substring "user $createOrRemove"

  # additional identifiers
  if [ "$command" == "setup" ]; then
    jqr '.userGroups[] | .additionalIdentifiers | to_entries[] | .key as $user | .value[]' \
      'pepcli --oauth-token-group "Access Administrator" user addIdentifier \($user | @sh) \(. | @sh)'
  else
    jqr '.userGroups[] | .additionalIdentifiers | to_entries[] | .value[]' \
      'pepcli --oauth-token-group "Access Administrator" user removeIdentifier \(. | @sh)'
  fi

  empty_line

  # column groups
  jqr '.columnGroups[]' \
    'pepcli --oauth-token-group "Data Administrator" ama columnGroup '"$createOrRemove"' \(.name | @sh)'

  # column group access rules
  jqr '.columnGroups[] | .name as $cGroup | .cgars | to_entries[]' \
    'pepcli --oauth-token-group "Access Administrator" ama cgar '"$createOrRemove"' \($cGroup | @sh) \(.key | @sh) \(.value[] | @sh)'

  empty_line

  # individual columns
  jqr '.columnGroups[] | .name as $group | .columns[]' \
    'pepcli --oauth-token-group "Data Administrator" ama column '"$createOrRemove"' \(. | @sh)' "\n" \
    'pepcli --oauth-token-group "Data Administrator" ama column '"$addOrRemove"' \(. | @sh) \($group | @sh)' |
    partition_by_substring "ama column $createOrRemove"

  empty_line

  # subject groups
  jqr '.subjectGroups[]' \
    'pepcli --oauth-token-group "Data Administrator" ama group '"$createOrRemove"' \(.name | @sh)'

  # subject group access rules
  jqr '.subjectGroups[] | .name as $cGroup | .pgars | to_entries[]' \
    'pepcli --oauth-token-group "Access Administrator" ama pgar '"$createOrRemove"' \($cGroup | @sh) \(.key | @sh) \(.value[] | @sh)'

  empty_line

  # individual subjects
  if [ "$command" == "setup" ]; then
    jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
      '\($bash_var)="$(' \
      "pepcli --oauth-token-group 'Data Administrator' register id | grep 'identifier:' | cut -d ':' -f2 | tr -d '[:space:]'" \
      ')"'
  fi

  jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
    'pepcli --oauth-token-group "Data Administrator" ama group '"$addOrRemove"' \($group | @sh) ${\($bash_var)}'

  if [ "$command" == "setup" ]; then
    empty_line

    jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var | .value | to_entries[]' \
      'pepcli --oauth-token-group "Data Administrator" store -p ${\($bash_var)} -c \(.key | @sh) -d \(.value | @sh)'
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
