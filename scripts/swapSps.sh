#!/usr/bin/env bash

# Swaps short pseudonym values between two participants.
# Usage: swapSps <path-to-cli-exe> <oauth-token> <sp-column-name> <pep-id-1> <pep-id-2>
# You'll need to specify an OAuth token with write access to the specified column,
# e.g. one for `RegistrationServer`.

set -o errexit
set -o nounset

cli="$1"
oauth="$2"
col="$3"
id_lhs="$4"
id_rhs="$5"

get_sp() {
  id="$1"
  "$cli" --oauth-token "$oauth" list -c "$col" -p "$id" 2> /dev/null | jq -r ".[0].data.\"$col\""
}

sp_lhs=$(get_sp "$id_lhs")
>&2 echo Participant "$id_lhs" had "$col" of "$sp_lhs"
sp_rhs=$(get_sp "$id_rhs")
>&2 echo Participant "$id_rhs" had "$col" of "$sp_rhs"

>&2 echo Clearing "$id_lhs"\'s "$col"
"$cli" --oauth-token "$oauth" store -p "$id_lhs" -c "$col" -d "" 2> /dev/null

>&2 echo Setting "$id_rhs"\'s "$col" to "$sp_lhs"
"$cli" --oauth-token "$oauth" store -p "$id_rhs" -c "$col" -d "$sp_lhs" 2> /dev/null

>&2 echo Setting "$id_lhs"\'s "$col" to "$sp_rhs"
"$cli" --oauth-token "$oauth" store -p "$id_lhs" -c "$col" -d "$sp_rhs" 2> /dev/null

sp_lhs=$(get_sp "$id_lhs")
>&2 echo Participant "$id_lhs" has "$col" of "$sp_lhs"
sp_rhs=$(get_sp "$id_rhs")
>&2 echo Participant "$id_rhs" has "$col" of "$sp_rhs"
