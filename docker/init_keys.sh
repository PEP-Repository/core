#!/usr/bin/env bash
readonly DATADIR="${1:-/data}"
readonly APPSDIR="${2:-/app}"
PUBLIC_KEYS="$("$APPSDIR/pepGenerateSystemKeys" both "$DATADIR/accessmanager/SystemKeys.json" "$DATADIR/transcryptor/SystemKeys.json")"
readonly PUBLIC_KEYS
PKDATA="$(echo "$PUBLIC_KEYS" | sed -n -e "s/PublicKeyData: \([0-9a-fA-F]*\)/\1/p")"
PKPSEUDONYMS="$(echo "$PUBLIC_KEYS" | sed -n -e "s/PublicKeyPseudonyms: \([0-9a-fA-F]*\)/\1/p")"


# Update client and registration server configuration with new public keys
sed -i -e "s/\"PublicKeyData\".*/\"PublicKeyData\": \"${PKDATA}\",/" "$DATADIR"/client/ClientConfig.json
sed -i -e "s/\"PublicKeyPseudonyms\".*/\"PublicKeyPseudonyms\": \"${PKPSEUDONYMS}\",/" "$DATADIR"/client/ClientConfig.json
sed -i -e "s/\"PublicKeyPseudonyms\".*/\"PublicKeyPseudonyms\": \"${PKPSEUDONYMS}\",/" "$DATADIR"/accessmanager/AccessManager.json
sed -i -e "s/dataPk.*/dataPk: ${PKDATA}/" "$DATADIR"/watchdog/constellation.yaml
sed -i -e "s/pseudonymPk.*/pseudonymPk: ${PKPSEUDONYMS}/" "$DATADIR"/watchdog/constellation.yaml

cat "$DATADIR"/client/ClientConfig.json
cat "$DATADIR"/watchdog/constellation.yaml

sed -i -e "s/\"PublicKeyData\".*/\"PublicKeyData\": \"${PKDATA}\",/" "$DATADIR"/registrationserver/RegistrationServer.json
sed -i -e "s/\"PublicKeyPseudonyms\".*/\"PublicKeyPseudonyms\": \"${PKPSEUDONYMS}\",/" "$DATADIR"/registrationserver/RegistrationServer.json

cat "$DATADIR"/registrationserver/RegistrationServer.json

# Generate key pair for the shadow administration
openssl genrsa -out "$DATADIR"/ShadowAdministration.key 2048
openssl rsa -in "$DATADIR"/ShadowAdministration.key -pubout -out "$DATADIR"/ShadowAdministration.pub
# Copy public key to expected locations
cp "$DATADIR"/ShadowAdministration.pub "$DATADIR"/client/ShadowAdministration.pub
