#!/usr/bin/env bash
# Source this script to obtain the signing certificate for macOS

set -euo pipefail

# Set the signing identity based on the provided argument
if [[ "${CI:-false}" == "true" ]]; then
  echo "Obtaining the signing certificate for macOS"

  required_vars=("GITLAB_CI_MACOS_CERTIFICATE_APP" \
                "GITLAB_CI_MACOS_DEVELOPER_NAME" \
                "GITLAB_CI_MACOS_DEVELOPER_ID" \
                "GITLAB_CI_MACOS_CERTIFICATE_INSTALL" \
                "GITLAB_CI_MACOS_CERTIFICATE_PWD" \
                "GITLAB_CI_MACOS_KEYCHAIN_PWD" \
                "GITLAB_CI_MACOS_CERTIFICATE_DEV_ID_CA" \
                "GITLAB_CI_MACOS_CERTIFICATE_APPLE_ROOT_CA"
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

  echo "Setting up pep keychain"

  # Delete existing pep keychain if it exists
  if [[ -f "$HOME/Library/Keychains/pep.keychain-db" ]]; then
    echo "Deleting existing pep keychain"
    security delete-keychain pep.keychain
  fi

  # Check if keychain exists, if not, create it
  if [[ ! -f "$HOME/Library/Keychains/pep.keychain-db" ]]; then
    echo "Creating new pep keychain"
    security -v create-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" pep.keychain
  else
    echo "Something went wrong, PEP keychain was not properly deleted. Check logs"
    exit 1
  fi

  # Add the keychain to the search list if not already present
  if ! security list-keychains -d user | grep -q "pep.keychain"; then
    echo "Adding pep keychain to search list"
    # shellcheck disable=SC2046
    security -v list-keychains -d user -s pep.keychain $(security list-keychains -d user | tr -d '"')
  else
    echo "PEP keychain already in search list."
  fi

  echo "Checking for application certificate in keychain"
  # Import certificates into the pep keychain only if they don't already exist
  if ! security find-identity -v -p codesigning pep.keychain | grep -q "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME"; then
    echo "Importing application certificate"
    echo "$GITLAB_CI_MACOS_CERTIFICATE_APP" | base64 --decode > app_cert.p12
    security -v import app_cert.p12 -k pep.keychain -P "$GITLAB_CI_MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign
    rm -f app_cert.p12
  else
    echo "Application certificate already exists in keychain"
  fi

  echo "Checking for installer certificate in keychain"
  if ! security find-identity -v -p codesigning pep.keychain | grep -q "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME"; then
    echo "Importing installer certificate"
    echo "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL" | base64 --decode > install_cert.p12
    security -v import install_cert.p12 -k pep.keychain -P "$GITLAB_CI_MACOS_CERTIFICATE_PWD" -T /usr/bin/productsign -T /usr/bin/pkgbuild -T /usr/bin/productbuild
    rm -f install_cert.p12
  else
    echo "Installer certificate already exists in keychain"
  fi

  echo "Checking for Developer ID CA certificate in keychain"
  if ! security find-certificate -c "Developer ID Certification Authority" pep.keychain >/dev/null 2>&1; then
    echo "Importing Developer ID CA certificate"
    echo "$GITLAB_CI_MACOS_CERTIFICATE_DEV_ID_CA" | base64 --decode > dev_id_ca.cer
    security -v import dev_id_ca.cer -k pep.keychain -A
    rm -f dev_id_ca.cer
  else
    echo "Developer ID CA certificate already exists in keychain"
  fi

  echo "Checking for Apple Root CA certificate in keychain"
  if ! security find-certificate -c "Apple Root CA" pep.keychain >/dev/null 2>&1; then
    echo "Importing Apple Root CA certificate"
    echo "$GITLAB_CI_MACOS_CERTIFICATE_APPLE_ROOT_CA" | base64 --decode > apple_root_ca.cer
    security -v import apple_root_ca.cer -k pep.keychain -A
    rm -f apple_root_ca.cer
  else
    echo "Apple Root CA certificate already exists in keychain"
  fi

  # Determine which operations are allowed without requiring the user to manually unlock the keychain.
  security -v set-key-partition-list -S apple-tool:,apple:,codesign:,productsign:,pkgbuild: -s -k "$GITLAB_CI_MACOS_KEYCHAIN_PWD" pep.keychain >/dev/null

  # Unlock the keychain
  security -v unlock-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" pep.keychain

  # And prevent relocking by both system sleep and timeout.
  security -v set-keychain-settings pep.keychain

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