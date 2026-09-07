#!/usr/bin/env bash

set -eu -o pipefail

data_dir=/data
pki_dir=/pki
storage_facility_bin=/app/pepStorageFacility
key_server_bin=/app/pepKeyServer
access_manager_bin=/app/pepAccessManager
transcryptor_bin=/app/pepTranscryptor
registration_server_bin=/app/pepRegistrationServer
authserver_bin=/app/pepAuthserver
pepcli_bin=/app/pepcli
pep_enrollment_bin=/app/pepEnrollment

while [ $# -gt 0 ]; do
  case "$1" in
    --data-dir) shift
      data_dir="$1" ;;
    --pki-dir) shift
      pki_dir="$1" ;;
    --storage-facility-bin) shift
      storage_facility_bin="$1" ;;
    --key-server-bin) shift
      key_server_bin="$1" ;;
    --access-manager-bin) shift
      access_manager_bin="$1" ;;
    --transcryptor-bin) shift
      transcryptor_bin="$1" ;;
    --registration-server-bin) shift
      registration_server_bin="$1" ;;
    --authserver-bin) shift
      authserver_bin="$1" ;;
    --pepcli-bin) shift
      pepcli_bin="$1" ;;
    --pep-enrollment-bin) shift
      pep_enrollment_bin="$1" ;;
    --loglevel) shift
      loglevel="$1" ;;
    -?*)
      >&2 echo "WARN: Unknown option: $1"
      exit 2 ;;
    *) break
  esac
  shift
done

if [ $# -gt 0 ]; then
  >&2 echo "No positional arguments expected: $*"
  exit 2
fi

bin_args=()
if [ -n "${loglevel-}" ]; then
  bin_args=(--loglevel "$loglevel")
fi

finish() {
  set +u
  kill "$SFPID" || true
  kill "$KSPID" || true
  kill "$AMPID" || true
  kill "$TSPID" || true
  kill "$RSPID" || true
  kill "$ASPID" || true
}

trap finish EXIT

cd "$data_dir/storagefacility"
"$storage_facility_bin" "${bin_args[@]}" "$data_dir/storagefacility/StorageFacility.json" 2> >(sed -u "s/^/[StorageFacility] /" >&2) > >(sed -u "s/^/[StorageFacility] /") &
SFPID=$!
cd "$data_dir/keyserver"
"$key_server_bin" "${bin_args[@]}" "$data_dir/keyserver/KeyServer.json" 2> >(sed -u "s/^/[KeyServer] /" >&2) > >(sed -u "s/^/[KeyServer] /") &
KSPID=$!
cd "$data_dir/accessmanager"
"$access_manager_bin" "${bin_args[@]}" "$data_dir/accessmanager/AccessManager.json" 2> >(sed -u "s/^/[AccessManager] /" >&2) > >(sed -u "s/^/[AccessManager] /") &
AMPID=$!
cd "$data_dir/authserver"
"$authserver_bin" "${bin_args[@]}" "$data_dir/authserver/Authserver.json" 2> >(sed -u "s/^/[Authserver] /" >&2) > >(sed -u "s/^/[Authserver] /") &
ASPID=$!

sleep 4

echo "Requesting verifiers"

(cd "$data_dir/transcryptor"; "$pepcli_bin" "${bin_args[@]}" --client-working-directory ../client verifiers > Verifiers.json)

cat "$data_dir/transcryptor/Verifiers.json"

sleep 2

cd "$data_dir/transcryptor"
"$transcryptor_bin" "${bin_args[@]}" "$data_dir/transcryptor/Transcryptor.json" 2> >(sed -u "s/^/[Transcryptor] /" >&2) > >(sed -u "s/^/[Transcryptor] /") &
TSPID=$!

cd "$data_dir/registrationserver"
"$registration_server_bin" "${bin_args[@]}" "$data_dir/registrationserver/RegistrationServer.json" 2> >(sed -u "s/^/[RegistrationServer] /" >&2) > >(sed -u "s/^/[RegistrationServer] /") &
RSPID=$!

echo "Enrolling Transcryptor"
(cd "$data_dir/transcryptor"; "$pep_enrollment_bin" "${bin_args[@]}" Transcryptor.json 4 "$pki_dir/PEPTranscryptor.key" "$pki_dir/PEPTranscryptor.chain" "$data_dir"/transcryptor/TranscryptorKeys.json)
cat "$data_dir/transcryptor/TranscryptorKeys.json"


echo "Restarting Transcryptor"
kill $TSPID
sleep 2 # Give OS time to release the port, preventing "Address already in use". See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2471#note_39395
cd "$data_dir/transcryptor"
"$transcryptor_bin" "${bin_args[@]}" "$data_dir/transcryptor/Transcryptor.json" 2> >(sed -u "s/^/[Transcryptor2] /" >&2) > >(sed -u "s/^/[Transcryptor2] /") &
TSPID=$!

sleep 2

echo "Enrolling Access Manager"
(cd "$data_dir/accessmanager"; "$pep_enrollment_bin" "${bin_args[@]}" AccessManager.json 3 "$pki_dir/PEPAccessManager.key" "$pki_dir/PEPAccessManager.chain" "$data_dir"/accessmanager/AccessManagerKeys.json)
cat "$data_dir/accessmanager/AccessManagerKeys.json"

echo "Enrolling Storage Facility"
(cd "$data_dir/storagefacility"; "$pep_enrollment_bin" "${bin_args[@]}" StorageFacility.json 2 "$pki_dir/PEPStorageFacility.key" "$pki_dir/PEPStorageFacility.chain" "$data_dir"/storagefacility/StorageFacilityKeys.json)
cat "$data_dir/storagefacility/StorageFacilityKeys.json"

echo "Enrolling Registration Server"
(cd "$data_dir/registrationserver"; "$pep_enrollment_bin" "${bin_args[@]}" RegistrationServer.json 5 "$pki_dir/PEPRegistrationServer.key" "$pki_dir/PEPRegistrationServer.chain" "$data_dir"/registrationserver/RegistrationServerKeys.json)
cat "$data_dir/registrationserver/RegistrationServerKeys.json"
