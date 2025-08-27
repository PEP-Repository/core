#!/usr/bin/env bash

PEP_KEYCHAIN="$HOME/Library/Keychains/pep.keychain-db"

# This script checks the notarization status of the specified input UUIDs.

set -exo pipefail

check_notarization() {

  # Store the credentials in the keychain
  xcrun notarytool store-credentials "notarytool-password" \
                  --apple-id "$GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID" \
                  --team-id "$GITLAB_CI_MACOS_DEVELOPER_ID" \
                  --password "$GITLAB_CI_MACOS_NOTARIZATION_PWD" \
                  --keychain "$PEP_KEYCHAIN"

  # Create a temporary file to signal an error
  ERROR_FILE=$(mktemp)

  for UUID in "$@"; do
    (
      for (( ; ; )); do
        local NOTARIZATION_STATUS_FILE
        NOTARIZATION_STATUS_FILE=$(mktemp)

        xcrun notarytool info "$UUID" \
          --keychain-profile "notarytool-password" \
          --keychain "$PEP_KEYCHAIN" \
          --output-format plist \
          > "$NOTARIZATION_STATUS_FILE"

        # something to get notarize status
        NOTARIZATION_STATUS="$(/usr/libexec/PlistBuddy -c "Print status" "$NOTARIZATION_STATUS_FILE")"
        echo "Notarization status: $NOTARIZATION_STATUS"
        case "$NOTARIZATION_STATUS" in
            "In Progress")
                echo "Waiting for notarization to complete: sleeping 10 seconds..."
                sleep 10
                ;;
            "Accepted")
                echo "File $EXPORT_PATH with $UUID: $NOTARIZATION_STATUS"
                echo "Notarization succeeded. Details:"
                xcrun notarytool log --keychain-profile "notarytool-password" "$UUID" \
                                     --keychain "$PEP_KEYCHAIN"
                exit 0
                ;;
            "Invalid")
                echo "File $EXPORT_PATH with $UUID: $NOTARIZATION_STATUS"
                echo "Notarization Invalid. Details:"
                xcrun notarytool log --keychain-profile "notarytool-password" "$UUID" \
                                     --keychain "$PEP_KEYCHAIN"
                echo 1 > "$ERROR_FILE"
                exit 1
                ;;
            *)
                echo "File $EXPORT_PATH with $UUID: $NOTARIZATION_STATUS"
                echo "Notarization failed with status: $NOTARIZATION_STATUS"
                echo 1 > "$ERROR_FILE"
                exit 1
                ;;
        esac
      done
    ) 2> >(sed -u "s/^/$UUID\t/" >&2) | sed -u "s/^/$UUID\t/" &
  done

  # Wait for all background jobs to finish
  wait

  # If any notarization check signaled an error, exit with an error status
  if [[ -s "$ERROR_FILE" ]]; then
    echo "Notarization failed"
    exit 1
  else
    echo "Notarization succeeded"
    exit 0
  fi
}

if [[ "${CI:-false}" == "true" ]]; then

  echo "Checking for required gitlab variables"

  required_vars=("GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID" \
                 "GITLAB_CI_MACOS_NOTARIZATION_PWD" \
                 "GITLAB_CI_MACOS_DEVELOPER_ID")

  # GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID: Contains the email adress used for the Apple Developer subscription
  # GITLAB_CI_MACOS_NOTARIZATION_PWD: Contains the app-specific password for notarytool
  # GITLAB_CI_MACOS_DEVELOPER_ID: Contains the Apple Developer Team ID

  for var in "${required_vars[@]}"; do
    if [[ -z "${!var}" ]]; then
      echo "Error: Required gitlab variable $var is not set"
      exit 1
    fi
  done
  echo "Required GitLab variables are set"

  check_notarization  "$@"
else
  if [[ $# -lt 4 ]]; then
    echo "Usage: $0 apple_id notarization_pwd developer_id notarization_uuid [notarization_uuid2 ...]"
    exit 1
  fi
  GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID=$1
  GITLAB_CI_MACOS_NOTARIZATION_PWD=$2
  GITLAB_CI_MACOS_DEVELOPER_ID=$3
  shift 3

  check_notarization "$@"
fi
