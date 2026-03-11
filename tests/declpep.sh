#!/usr/bin/env bash

json_file="$1"
if [ ! -f "$json_file" ]; then
  echo "Error: JSON file not found."
  exit 1
fi

# user groups
jq -r '.userGroups[] |
  "pepcli --oauth-token-group \"Access Administrator\" user group create \(.)"' \
  "$json_file"

echo ""

# column groups
jq -r '.columnGroups[] |
  "pepcli --oauth-token-group \"Data Administrator\" ama columnGroup create \(.name)"' \
  "$json_file"

# column group access rules
jq -r '.columnGroups[] | .name as $group | .cgars[] |
  "pepcli --oauth-token-group \"Access Administrator\" ama cgar create \($group) \(.userGroup) \(.permissions[])"' \
  "$json_file"

echo ""

# individual columns
jq -r '.columnGroups[] | .name as $group | .columns[] |
  "pepcli --oauth-token-group \"Data Administrator\" ama column create \(.)\npepcli --oauth-token-group \"Data Administrator\" ama column addTo \(.) \($group)"' \
  "$json_file"

echo ""

# subject groups
jq -r '.participantGroups[] |
  "pepcli --oauth-token-group \"Data Administrator\" ama group create \(.name)"' \
  "$json_file"

# subject group access rules
jq -r '.participantGroups[].pgars[] |
  "pepcli --oauth-token-group \"Access Administrator\" ama pgar create \(.userGroup) \(.permissions[])"' \
  "$json_file"

echo ""

# individual subjects
jq -r '.participantGroups[] | .name as $group | .participants | to_entries[] | "ID_\($group)_\(.key)" as $bash_var |
  "\($bash_var)=\"$(pepcli --oauth-token-group \"Data Administrator\" register id)\"\npepcli --oauth-token-group \"Data Administrator\" ama group addTo \($group) \"${\($bash_var)}\""' \
  "$json_file"

echo ""

jq -r '.participantGroups[] | .name as $group | .participants | to_entries[] | "ID_\($group)_\(.key)" as $bash_var | .value | to_entries[] |
  "pepcli --oauth-token-group \"Data Administrator\" store -p \"${\($bash_var)}\" -c \"\(.key)\" -d \"\(.value)\""' \
  "$json_file"

