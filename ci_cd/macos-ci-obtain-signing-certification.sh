#!/usr/bin/env bash
# Source this script to obtain the signing certificate for macOS

set -e

# Set the signing identity based on the provided argument
if [[ "${CI:-false}" == "true" ]]; then
  echo "Obtaining the signing certificate for macOS"

  required_vars=("GITLAB_CI_MACOS_CERTIFICATE_APP" \
                "GITLAB_CI_MACOS_DEVELOPER_NAME" \
                "GITLAB_CI_MACOS_DEVELOPER_ID" \
                "GITLAB_CI_MACOS_CERTIFICATE_INSTALL" \
                "GITLAB_CI_MACOS_CERTIFICATE_PWD" \
                "GITLAB_CI_MACOS_KEYCHAIN_PWD" \
                "GITLAB_CI_MACOS_SYSTEM_PWD" \
                # "GITLAB_CI_MACOS_CERTIFICATE_DEV_ID_CA"
                # "GITLAB_CI_MACOS_CERTIFICATE_APPLE_ROOT_CA"
                )

  # GITLAB_CI_MACOS_CERTIFICATE_APP: Contains the the base64 of the application certificate
  # GITLAB_CI_MACOS_CERTIFICATE_APP_NAME: Contains the full certificate name, such as Developer ID Application: Your Name (K1234567)
  # GITLAB_CI_MACOS_CERTIFICATE_INSTALL: Contains the base64 of the installer certificate
  # GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME: Contains the full certificate name, such as Developer ID Installer: Your Name (K1234567)
  # GITLAB_CI_MACOS_CERTIFICATE_PWD: Contains the password entered when exporting the certificate from the Keychain Access app
  # GITLAB_CI_MACOS_KEYCHAIN_PWD: Contains a randomly generated password for the temporary keychain
  # GITLAB_CI_MACOS_CERTIFICATE_DEV_ID_CA: Contains the base64 of the Developer ID Certification Authority certificate
  # GITLAB_CI_MACOS_CERTIFICATE_APPLE_ROOT_CA: Contains the base64 of the Apple Root CA certificate
  
  for var in "${required_vars[@]}"; do
    if [[ -z "${!var}" ]]; then
      echo "Error: Required gitlab variable $var is not set"
      exit 1
    fi
  done
  echo "Required GitLab variables are set"

  GITLAB_CI_MACOS_DEV_NAME_AND_ID="$GITLAB_CI_MACOS_DEVELOPER_NAME ($GITLAB_CI_MACOS_DEVELOPER_ID)"
  GITLAB_CI_MACOS_CERTIFICATE_APP_NAME="Developer ID Application: $GITLAB_CI_MACOS_DEV_NAME_AND_ID"
  GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME="Developer ID Installer: $GITLAB_CI_MACOS_DEV_NAME_AND_ID"

  export GITLAB_CI_MACOS_CERTIFICATE_APP_NAME
  export GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME

  # Determine which operations are allowed without requiring the user to manually unlock the keychain.
  security set-key-partition-list -S apple-tool:,apple:,codesign:,productsign,pkgbuild: -s -k "$GITLAB_CI_MACOS_SYSTEM_PWD" login.keychain >/dev/null

  # Unlock the keychain
  security unlock-keychain -p "$GITLAB_CI_MACOS_SYSTEM_PWD" login.keychain

  # And prevent relocking by both system sleep and timeout.
  security set-keychain-settings login.keychain

else

  # Check for required gitlab variables
  echo "Running macOS signing certification script locally."
  echo "Importing required secrets, make sure local DTAP secrets are up-to-date."

  PEP_MACOS_DTAP_DIR="$(dirname "$(git rev-parse --git-common-dir)")"/../ops
  
  GITLAB_CI_MACOS_CERTIFICATE_APP_NAME=$(cat "$PEP_MACOS_DTAP_DIR/passwords/macos-signing-id-application.txt")
  GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME=$(cat "$PEP_MACOS_DTAP_DIR/passwords/macos-signing-id-install.txt")
  GITLAB_CI_MACOS_KEYCHAIN_PWD=$(cat "$PEP_MACOS_DTAP_DIR/passwords/macos-keychain-pw.txt")

  # Unlock the keychain
  security unlock-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" pep.keychain
  security set-keychain-settings -t 5400 -u pep.keychain
  
  security set-key-partition-list -S apple-tool:,apple:,codesign:,productsign: -s -k "$GITLAB_CI_MACOS_KEYCHAIN_PWD" pep.keychain >/dev/null

  export GITLAB_CI_MACOS_CERTIFICATE_APP_NAME
  export GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME

fi

echo "Successfully imported the certificates and unlocked the keychain."