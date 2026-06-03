#!/usr/bin/env bash
set -eux

readonly REUSE_SECRETS="${1:-false}"
readonly DATADIR="${2:-/data}"
readonly APPSDIR="${3:-/app}"
if [ "$REUSE_SECRETS" = false ]; then
  "$APPSDIR/pepGenerateSystemKeys" both "$DATADIR/accessmanager/SystemKeys.json" "$DATADIR/transcryptor/SystemKeys.json" > "$DATADIR/SystemPublicKeys.txt"
fi
PKDATA="$(sed -n -e "s/PublicKeyData: \([0-9a-fA-F]*\)/\1/p" "$DATADIR/SystemPublicKeys.txt")"
PKPSEUDONYMS="$(sed -n -e "s/PublicKeyPseudonyms: \([0-9a-fA-F]*\)/\1/p" "$DATADIR/SystemPublicKeys.txt")"


# Update client and server configurations with new public keys
add_system_public_keys() {
  local config_file="$1"
  # jq is not available in pep-services image for integration.sh, so use sed instead
  sed -i \
    -e "s/\"PublicKeyData\":\s*\"\"/\"PublicKeyData\": \"${PKDATA}\"/" \
    -e "s/\"PublicKeyPseudonyms\":\s*\"\"/\"PublicKeyPseudonyms\": \"${PKPSEUDONYMS}\"/" \
    -- "$config_file"
}
add_system_public_keys "$DATADIR/client/ClientConfig.json"
add_system_public_keys "$DATADIR/accessmanager/AccessManager.json"
add_system_public_keys "$DATADIR/transcryptor/Transcryptor.json"
add_system_public_keys "$DATADIR/registrationserver/RegistrationServer.json"

sed -i -e "s/dataPk.*/dataPk: ${PKDATA}/" "$DATADIR"/watchdog/constellation.yaml
sed -i -e "s/pseudonymPk.*/pseudonymPk: ${PKPSEUDONYMS}/" "$DATADIR"/watchdog/constellation.yaml

cat "$DATADIR"/client/ClientConfig.json
cat "$DATADIR"/watchdog/constellation.yaml
cat "$DATADIR"/registrationserver/RegistrationServer.json

if [ "$REUSE_SECRETS" = false ]; then
  # Generate key pair for the shadow administration
  openssl genrsa -out "$DATADIR"/ShadowAdministration.key 2048
  openssl rsa -in "$DATADIR"/ShadowAdministration.key -pubout -out "$DATADIR"/ShadowAdministration.pub
  # Copy public key to expected locations
  cp "$DATADIR"/ShadowAdministration.pub "$DATADIR"/client/ShadowAdministration.pub
fi
