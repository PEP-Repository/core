#!/usr/bin/env bash
set -exuo pipefail

PEP_MACOS_APP_PATH="$1"
PEP_MACOS_APP_PARENT="$(dirname "$(readlink -f "$PEP_MACOS_APP_PATH")")"

echo "Generating appcast for $PEP_MACOS_APP_PATH"

# Create zip archive which is uploaded to the website and used to generate appcast
ditto -c -k --sequesterRsrc --keepParent "$PEP_MACOS_APP_PATH" "$PEP_MACOS_APP_PATH.zip"

# Delete the non .zip file
rm -rf "$PEP_MACOS_APP_PATH"

# Find the Sparkle installation directory
SPARKLE_DIR="$(readlink -f "$(dirname "$(readlink -f "$(which sparkle)")")/../../..")"

if [[ -z "$SPARKLE_DIR" ]]; then
  echo "Sparkle not found"
  exit 1
fi

# use xattr -d com.apple.quarantine <file> to remove the quarantine attribute from generate appcast which makes it noninteractive
if xattr -p com.apple.quarantine "$SPARKLE_DIR/bin/generate_appcast" &> /dev/null; then
    xattr -d com.apple.quarantine "$SPARKLE_DIR/bin/generate_appcast"
fi

clean_up_sparkle() {
  rm -rf $HOME/Library/Caches/Sparkle_generate_appcast/*
}

# Generate the appcast
if [[ "${CI:-false}" == "true" ]]; then
  # Obtain sparkle key from CI
  if [[ -e "$CI_PROJECT_DIR/$PEP_DTAP_REPO_DIR/keys/sparklepriv.txt" ]]; then
      SPARKLE_KEY=$(cat "$CI_PROJECT_DIR/$PEP_DTAP_REPO_DIR/keys/sparklepriv.txt")
  else
      echo "Error: Sparkle private key path not found"
      exit 1
  fi

  if [[ -z "$SPARKLE_KEY" ]]; then
    echo "Error: Unable to retrieve Sparkle key from file"
    exit 1
  fi

  echo "$SPARKLE_KEY" | "$SPARKLE_DIR/bin/generate_appcast" --ed-key-file - \
  --link "https://pep.cs.ru.nl" \
  --critical-update-version "" \
  --auto-prune-update-files \
  -o "$PEP_MACOS_APP_PARENT/AppCast.xml" \
  "$PEP_MACOS_APP_PARENT"

  clean_up_sparkle
else
  source "$(git rev-parse --show-toplevel)/ci_cd/macos-ci-obtain-signing-certification.sh"

  "$SPARKLE_DIR/bin/generate_appcast" \
  --link "https://pep.cs.ru.nl" \
  --critical-update-version "" \
  --auto-prune-update-files \
  -o "$PEP_MACOS_APP_PARENT/AppCast.xml" \
  "$PEP_MACOS_APP_PARENT"

  clean_up_sparkle
fi