#!/usr/bin/env sh
set -eu
versionFile=$1

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
PEP_CORE_DIR="$SCRIPTPATH/.."

majorVersion="$($PEP_CORE_DIR/scripts/parse-version.sh get-major "$versionFile")"
minorVersion="$($PEP_CORE_DIR/scripts/parse-version.sh get-minor "$versionFile")"

echo "MAJOR_VERSION=$majorVersion" >> "$CI_PROJECT_DIR/version.env"
echo "MINOR_VERSION=$minorVersion" >> "$CI_PROJECT_DIR/version.env"
