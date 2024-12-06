#!/usr/bin/env bash
# shellcheck disable=SC2164

(cd /data/storagefacility; /app/pepStorageFacility StorageFacility.json) &
(cd /data/keyserver; /app/pepKeyServer KeyServer.json) &
(cd /data/transcryptor; /app/pepTranscryptor Transcryptor.json) &

sleep 2

(cd /data/accessmanager; /app/pepAccessManager AccessManager.json) &

sleep 2

(cd /data/registrationserver; /app/pepRegistrationServer RegistrationServer.json) &
(cd /data/authserver; /app/pepAuthserver Authserver.json) || exit
