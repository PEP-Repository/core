#!/usr/bin/env bash
# shellcheck disable=SC2016 # Reason: we are intentionally using single quoted strings to avoid expansion

json_file="$1"
if [ ! -f "$json_file" ]; then
  echo "Error: JSON file not found."
  exit 1
fi

# Runs a jq filter on $json_file and then feeds this into a jq format string
# The first argument specifies the filter
# The other arguments are concatenated into a single format string
jqr() {
  # Removes leading and trailing whitespace from each line of input.
  trim() {
    sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
  }

  local filter=$1
  shift
  local genlines=${*//\"/\\\"}

  jq -r "$filter | \"$genlines\"" "$json_file" | trim
}

# Prints an empty line
empty_line() {
  echo
}

# Move lines with the substring before those without it
partition_by_substring() {
  readonly substring="$1"

  local tempfile
  tempfile=$(mktemp) && trap 'rm -f "$tempfile"' RETURN

  tee "$tempfile" | grep -e "$substring"
  cat "$tempfile" | grep -v "$substring"
}

# user groups
jqr '.userGroups[]' \
  'pepcli --oauth-token-group "Access Administrator" user group create \(.)'

empty_line

# column groups
jqr '.columnGroups[]' \
  'pepcli --oauth-token-group "Data Administrator" ama columnGroup create \(.name)'

# column group access rules
jqr '.columnGroups[] | .name as $group | .cgars[]' \
  'pepcli --oauth-token-group "Access Administrator" ama cgar create \($group) \(.userGroup) \(.permissions[])'

empty_line

# individual columns
jqr '.columnGroups[] | .name as $group | .columns[]' \
  'pepcli --oauth-token-group "Data Administrator" ama column create \(.)' "\n" \
  'pepcli --oauth-token-group "Data Administrator" ama column addTo \(.) \($group)'

empty_line

# subject groups
jqr '.subjectGroups[]' \
  'pepcli --oauth-token-group "Data Administrator" ama group create \(.name)'

# subject group access rules
jqr '.subjectGroups[] | .name as $group | .pgars[]' \
  'pepcli --oauth-token-group "Access Administrator" ama pgar create \($group) \(.userGroup) \(.permissions[])'

empty_line

# individual subjects
jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var' \
  '\($bash_var)="$(pepcli --oauth-token-group "Data Administrator" register id)"' "\n" \
  'pepcli --oauth-token-group "Data Administrator" ama group addTo \($group) "${\($bash_var)}"' |
  partition_by_substring "register id"

empty_line

jqr '.subjectGroups[] | .name as $group | .subjects | to_entries[] | "ID_\($group)_\(.key)" as $bash_var | .value | to_entries[]' \
  'pepcli --oauth-token-group "Data Administrator" store -p "${\($bash_var)}" -c "\(.key)" -d "\(.value)"'
