#!/usr/bin/env bash

# Sources:
# https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow
# https://federicoterzi.com/blog/automatic-code-signing-and-notarization-for-macos-apps-using-github-actions/

set -exuo pipefail

PEP_KEYCHAIN="$HOME/Library/Keychains/pep.keychain-db"

notarize_macos_apps() {
  local PRODUCT_PATHS=("$@")
  local NOTARIZATION_UUIDS=()

  echo "Notarizing macOS apps"

  xcrun notarytool store-credentials \
                  --verbose  \
                  --apple-id "$GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID" \
                  --team-id "$GITLAB_CI_MACOS_DEVELOPER_ID" \
                  --password "$GITLAB_CI_MACOS_NOTARIZATION_PWD" \
                  "notarytool-password" \
                  --keychain "$PEP_KEYCHAIN"

  for PRODUCT_PATH in "${PRODUCT_PATHS[@]}"; do

    local SUCCESS_FILE
    SUCCESS_FILE="$(mktemp)"

    echo "Notarizing $PRODUCT_PATH"

    # Compress product path for upload with ditto, if it is an .app, zip with --keepParent
    if [[ -d "$PRODUCT_PATH" ]]; then
      ditto -c -k --sequesterRsrc --keepParent "$PRODUCT_PATH" "$PRODUCT_PATH.zip"
    else
      ditto -c -k --sequesterRsrc "$PRODUCT_PATH" "$PRODUCT_PATH.zip"
    fi

    for i in {1..10}; do
      # Notarytool sometimes randomly kills the shell with bus error 10, a few retry attempts in a subshell seems to fix it.
      set +e
      (
        xcrun notarytool submit --verbose \
                                --keychain-profile "notarytool-password" \
                                --output-format plist \
                                "$PRODUCT_PATH.zip" > "notarization_submit.plist" && echo 1 > "$SUCCESS_FILE"
      )
      if [[ -s "$SUCCESS_FILE" ]]; then
        break
      else
        echo "($i/10) Notarization request failed, retrying in 5 seconds"
        sleep 5
      fi
      set -e
    done

    # Remove the temporary zip file
    rm -f "$PRODUCT_PATH.zip"

    if [[ ! -s "$SUCCESS_FILE" ]]; then
      echo "Notarization request failed after 10 attempts"
      exit 1
    fi

    local MACOS_NOTARIZATION_REQUEST_UUID
    MACOS_NOTARIZATION_REQUEST_UUID="$(/usr/libexec/PlistBuddy -c "Print id" "notarization_submit.plist")"
    echo "Notarization request UUID: ${MACOS_NOTARIZATION_REQUEST_UUID}"
    NOTARIZATION_UUIDS+=("$MACOS_NOTARIZATION_REQUEST_UUID")
  done

  # Export NOTARIZATION_UUIDS as an environment variable
  echo "${NOTARIZATION_UUIDS[*]}" > notarization_uuids.txt
}

if [[ "${CI:-false}" == "true" ]]; then

  # Required GitLab variables:
  # GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID: Contains the email adress used for the Apple Developer subscription
  # GITLAB_CI_MACOS_NOTARIZATION_PWD: Contains the app-specific password for notarytool
  # GITLAB_CI_MACOS_DEVELOPER_ID: Contains the Apple Developer Team ID

  echo "Notarizing the following products: $*"
  notarize_macos_apps "$@"
else
  if [[ $# -lt 4 ]]; then
    echo "Usage: $0 apple_id notarization_pwd developer_id path_to_app1 [path_to_app2 ...]"
    exit 1
  fi
  GITLAB_CI_MACOS_NOTARIZATION_APPLE_ID=$1
  GITLAB_CI_MACOS_NOTARIZATION_PWD=$2
  GITLAB_CI_MACOS_DEVELOPER_ID=$3
  shift 3

  notarize_macos_apps "$@"
fi