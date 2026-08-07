#!/usr/bin/env bash
set -eu -o pipefail

# Update Transcryptor Verifiers.json from PEP 1.6 to 1.7
# See https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/2407

existing_file="$1"
pepcli_command="${2:-pepcli}"

if jq --exit-status .accessManager.reshuffleCommitment -- "$existing_file" >/dev/null; then
  # File contains new verifiers format
  >&2 echo 'Already updated'
  exit
fi

old_verifiers="$(jq --sort-keys . -- "$existing_file")"

new_verifiers="$($pepcli_command verifiers | jq)"
if printf %s "$new_verifiers" | jq --exit-status .accessManager.zb >/dev/null; then
  # pepcli returned old verifiers format
  >&2 echo 'Old pepcli version detected'
  exit 1
fi

new_verifiers_subset_mapped_to_old="$(printf %s "$new_verifiers" | jq --sort-keys '
  map_values({
    zOverKb: .reshuffleOverRekeyCommitment,
    zb: .reshuffleCommitment,
    ky: .rekeyedPublicKey
  })
')"
if ! diff <(printf %s "$old_verifiers") <(printf %s "$new_verifiers_subset_mapped_to_old"); then
  >&2 echo 'New verifiers do not match existing verifiers!'
  exit 1
fi

printf %s "$new_verifiers" >"$existing_file"
>&2 echo 'Verifiers updated.'
