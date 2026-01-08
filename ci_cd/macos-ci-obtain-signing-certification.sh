#!/usr/bin/env bash
# Source this script to obtain the signing certificate for macOS

set -euo pipefail

PEP_KEYCHAIN="$HOME/Library/Keychains/pep.keychain-db"

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
  if [[ -f "$PEP_KEYCHAIN" ]]; then
    echo "Deleting existing pep keychain"
    security delete-keychain "$PEP_KEYCHAIN"
  fi

  # Check if keychain exists, if not, create it
  if [[ ! -f "$PEP_KEYCHAIN" ]]; then
    echo "Creating new pep keychain"
    security -v create-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" "$PEP_KEYCHAIN"
  else
    echo "Something went wrong, PEP keychain was not properly deleted. Check logs"
    exit 1
  fi

  # Unlock the keychain
  security -v unlock-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" "$PEP_KEYCHAIN"

  # Add the keychain to the search list if not already present
  if ! security list-keychains -d user | grep -q "$PEP_KEYCHAIN"; then
    echo "Adding pep keychain to search list"
    # shellcheck disable=SC2046
    security -v list-keychains -d user -s "$PEP_KEYCHAIN" $(security list-keychains -d user | tr -d '"')
  else
    echo "PEP keychain already in search list."
  fi

  install_cert_if_missing() {
    local cert_base64="$1"
    local cert_file="$2"
    local keychain="$3"
    local check_type="$4"      # "identity" or "certificate"
    local check_value="$5"     # Name/identifier to check for
    shift 5
    local security_args=("$@")

    echo "Checking for $check_value in keychain"
    
    local found=1
    if [[ "$check_type" == "identity" ]]; then
      if security find-identity -v -p codesigning "$keychain" | grep -F "$check_value"; then
        found=0
      fi
    elif [[ "$check_type" == "certificate" ]]; then
      if security find-certificate -c "$check_value" "$keychain"; then
        found=0
      fi
    fi

    if [[ $found -eq 0 ]]; then
      echo "$check_value already exists in keychain"
      return 0
    fi

    echo "Importing $check_value"
    echo "$cert_base64" | base64 --decode > "$cert_file"
    security -v import "$cert_file" -k "$keychain" "${security_args[@]}"
    rm -f "$cert_file"
  }

  install_cert_if_missing "$GITLAB_CI_MACOS_CERTIFICATE_APPLE_ROOT_CA" apple_root_ca.cer "$PEP_KEYCHAIN" certificate "Apple Root CA" -A
  install_cert_if_missing "$GITLAB_CI_MACOS_CERTIFICATE_DEV_ID_CA" dev_id_ca.cer "$PEP_KEYCHAIN" certificate "Developer ID Certification Authority" -A
  install_cert_if_missing "$GITLAB_CI_MACOS_CERTIFICATE_APP" app_cert.p12 "$PEP_KEYCHAIN" identity "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" -P "$GITLAB_CI_MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign
  install_cert_if_missing "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL" install_cert.p12 "$PEP_KEYCHAIN" identity "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME" -P "$GITLAB_CI_MACOS_CERTIFICATE_PWD" -T /usr/bin/productsign -T /usr/bin/pkgbuild -T /usr/bin/productbuild

  # Determine which operations are allowed without requiring the user to manually unlock the keychain.
  security -v set-key-partition-list -S apple-tool:,apple:,codesign:,productsign:,pkgbuild:,productbuild: -s -k "$GITLAB_CI_MACOS_KEYCHAIN_PWD" "$PEP_KEYCHAIN" >/dev/null

  # And prevent relocking by both system sleep and timeout.
  security -v set-keychain-settings "$PEP_KEYCHAIN"

else

  # Check for required gitlab variables
  echo "Running macOS signing certification script locally."
  echo "Importing required secrets, make sure local OPS secrets are up-to-date."

  PEP_MACOS_OPS_DIR="$(dirname "$(git rev-parse --git-common-dir)")"/../ops
  
  GITLAB_CI_MACOS_CERTIFICATE_APP_NAME=$(cat "$PEP_MACOS_OPS_DIR/passwords/macos-signing-id-application.txt")
  GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME=$(cat "$PEP_MACOS_OPS_DIR/passwords/macos-signing-id-install.txt")
  GITLAB_CI_MACOS_KEYCHAIN_PWD=$(cat "$PEP_MACOS_OPS_DIR/passwords/macos-keychain-pw.txt")

  # Unlock the keychain
  security unlock-keychain -p "$GITLAB_CI_MACOS_KEYCHAIN_PWD" "$PEP_KEYCHAIN"
  security set-keychain-settings -t 5400 -u "$PEP_KEYCHAIN"
  
  security set-key-partition-list -S apple-tool:,apple:,codesign:,productsign:,pkgbuild:,productbuild: -s -k "$GITLAB_CI_MACOS_KEYCHAIN_PWD" "$PEP_KEYCHAIN" >/dev/null

  export GITLAB_CI_MACOS_CERTIFICATE_APP_NAME
  export GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME

fi

echo "Successfully imported the certificates and unlocked the keychain."