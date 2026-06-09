#!/usr/bin/env bash

# Example: ./scripts/validate-config.sh ./config/local/

set -eu

script_dir="$(realpath "$(dirname -- "$0")")"

config_file_or_folder="$1"

schema="$script_dir/../config/config.schema.json"
validate_config_file() {
  local config_file="$1"
  if command -v jv >/dev/null 2>&1; then
    jv -- "$schema" "$config_file"
  elif command -v check-jsonschema >/dev/null 2>&1; then
    check-jsonschema --schemafile "$schema" -- "$config_file"
  elif command -v jsonschema >/dev/null 2>&1; then
    jsonschema -i "$config_file" -- "$schema"
  else
    >&2 echo "No JSON schema validator found"
    exit 1
  fi
}

if [ -d "$config_file_or_folder" ]; then
  cd "$config_file_or_folder"

  # Find config files regardless of directory structure (e.g. pep-services subfolder).
  # Also include StorageFacility.local.json etc. for integration test config.
  find -name ClientConfig.json \
      -or -name AccessManager.json \
      -or -name Authserver.json \
      -or -name KeyServer.json \
      -or -name RegistrationServer.json \
      -or -name StorageFacility.json \
      -or -name 'StorageFacility.*.json' \
      -or -name Transcryptor.json |
  while read -r config_file; do
    if [ -f "$config_file" ]; then
      if ! validate_config_file "$config_file"; then
        echo "^ $config_file failed to validate"
        exit 1
      fi
    fi
  done
else
  validate_config_file "$config_file_or_folder"
fi
