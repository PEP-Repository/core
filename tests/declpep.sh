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
  local -r filter=$1
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
    'pepcli --oauth-token-group "Access Administrator" user group '"$createOrRemove"' "\(.name)"'

  # individual users
  jqr '.userGroups[] | .name as $group | .users[]' \
    'pepcli --oauth-token-group "Access Administrator" user '"$createOrRemove"' "\(.)"' "\n" \
    'pepcli --oauth-token-group "Access Administrator" user '"$addOrRemove"' "\(.)" "\($group)"' |
    partition_by_substring "ama column $createOrRemove"

  empty_line

  # column groups
  jqr '.columnGroups[]' \
    'pepcli --oauth-token-group "Data Administrator" ama columnGroup '"$createOrRemove"' "\(.name)"'

  # column group access rules
  jqr '.columnGroups[] | .name as $group | .cgars[]' \
    'pepcli --oauth-token-group "Access Administrator" ama cgar '"$createOrRemove"' "\($group)" "\(.userGroup)" "\(.permissions[])"'

  empty_line

  # individual columns
  jqr '.columnGroups[] | .name as $group | .columns[]' \
    'pepcli --oauth-token-group "Data Administrator" ama column '"$createOrRemove"' "\(.)"' "\n" \
    'pepcli --oauth-token-group "Data Administrator" ama column '"$addOrRemove"' "\(.)" "\($group)"' |
    partition_by_substring "ama column $createOrRemove"

  empty_line

  # subject groups
  jqr '.subjectGroups[]' \
    'pepcli --oauth-token-group "Data Administrator" ama group '"$createOrRemove"' "\(.name)"'

  # subject group access rules
  jqr '.subjectGroups[] | .name as $group | .pgars[]' \
    'pepcli --oauth-token-group "Access Administrator" ama pgar '"$createOrRemove"' "\($group)" "\(.userGroup)" "\(.permissions[])"'

  empty_line

  # individual subjects
  if [ "$command" == "setup" ]; then
    jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
      '\($bash_var)="$(' \
      "pepcli --oauth-token-group 'Data Administrator' register id | grep 'identifier:' | cut -d ':' -f2 | tr -d '[:space:]'" \
      ')"'
  fi

  jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
    'pepcli --oauth-token-group "Data Administrator" ama group '"$addOrRemove"' "\($group)" "${\($bash_var)}"'

  if [ "$command" == "setup" ]; then
    empty_line

    jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var | .value | to_entries[]' \
      'pepcli --oauth-token-group "Data Administrator" store -p "${\($bash_var)}" -c "\(.key)" -d "\(.value)"'
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
