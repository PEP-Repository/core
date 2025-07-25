#!/usr/bin/env bash

set -o errexit
set -o pipefail

finish() {
  kill "$SFPID" || true
  kill "$KSPID" || true
  kill "$AMPID" || true
  kill "$TSPID" || true
  kill "$RSPID" || true
  kill "$ASPID" || true
}

trap finish EXIT

readonly DATADIR="${1:-/data}"
readonly PKIDIR="${2:-/pki}"
readonly STORAGEFACILITY="${3:-/app/pepStorageFacility}"
readonly KEYSERVER="${4:-/app/pepKeyServer}"
readonly ACCESSMANAGER="${5:-/app/pepAccessManager}"
readonly TRANSCRYPTOR="${6:-/app/pepTranscryptor}"
readonly REGISTRATIONSERVER="${7:-/app/pepRegistrationServer}"
readonly AUTHSERVER="${8:-/app/pepAuthserver}"
readonly CLI="${9:-/app/pepcli}"
readonly ENROLLMENT="${10:-/app/pepEnrollment}"

cd "$DATADIR/storagefacility"
"$STORAGEFACILITY" "$DATADIR/storagefacility/StorageFacility.json" &
SFPID=$!
cd "$DATADIR/keyserver"
"$KEYSERVER" "$DATADIR/keyserver/KeyServer.json" &
KSPID=$!
cd "$DATADIR/accessmanager"
"$ACCESSMANAGER" "$DATADIR/accessmanager/AccessManager.json" &
AMPID=$!
cd "$DATADIR/authserver"
"$AUTHSERVER" "$DATADIR/authserver/Authserver.json" &
ASPID=$!

sleep 4

echo "Requesting verifiers"

(cd "$DATADIR/transcryptor"; "$CLI" --client-working-directory ../client verifiers > Verifiers.json)

cat "$DATADIR/transcryptor/Verifiers.json"

sleep 2

cd "$DATADIR/transcryptor"
"$TRANSCRYPTOR" "$DATADIR/transcryptor/Transcryptor.json" &
TSPID=$!

cd "$DATADIR/registrationserver"
"$REGISTRATIONSERVER" "$DATADIR/registrationserver/RegistrationServer.json" &
RSPID=$!

echo "Enrolling Transcryptor"
(cd "$DATADIR/transcryptor"; "$ENROLLMENT" Transcryptor.json 4 "$PKIDIR/PEPTranscryptor.key" "$PKIDIR/PEPTranscryptor.chain" "$DATADIR"/transcryptor/TranscryptorKeys.json)
cat "$DATADIR/transcryptor/TranscryptorKeys.json"


echo "Restarting Transcryptor"
kill $TSPID
sleep 2 # Give OS time to release the port, preventing "Address already in use". See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2471#note_39395
cd "$DATADIR/transcryptor"
"$TRANSCRYPTOR" "$DATADIR/transcryptor/Transcryptor.json" &
TSPID=$!

sleep 2

echo "Enrolling Access Manager"
(cd "$DATADIR/accessmanager"; "$ENROLLMENT" AccessManager.json 3 "$PKIDIR/PEPAccessManager.key" "$PKIDIR/PEPAccessManager.chain" "$DATADIR"/accessmanager/AccessManagerKeys.json)
cat "$DATADIR/accessmanager/AccessManagerKeys.json"

echo "Enrolling Storage Facility"
(cd "$DATADIR/storagefacility"; "$ENROLLMENT" StorageFacility.json 2 "$PKIDIR/PEPStorageFacility.key" "$PKIDIR/PEPStorageFacility.chain" "$DATADIR"/storagefacility/StorageFacilityKeys.json)
cat "$DATADIR/storagefacility/StorageFacilityKeys.json"

echo "Enrolling Registration Server"
(cd "$DATADIR/registrationserver"; "$ENROLLMENT" RegistrationServer.json 5 "$PKIDIR/PEPRegistrationServer.key" "$PKIDIR/PEPRegistrationServer.chain" "$DATADIR"/registrationserver/RegistrationServerKeys.json)
cat "$DATADIR/registrationserver/RegistrationServerKeys.json"
