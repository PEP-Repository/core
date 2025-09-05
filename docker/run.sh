#!/usr/bin/env bash
set -eu

stop_jobs() {
  jobs="$(jobs -rp)"
  if [ -n "$jobs" ]; then
    # shellcheck disable=SC2086 # Split PIDs
    kill $jobs || true
    # shellcheck disable=SC2086 # Split PIDs
    wait -f $jobs || true
  fi
}
trap stop_jobs EXIT

(cd /data/storagefacility; /app/pepStorageFacility StorageFacility.json) 2> >(sed -u "s/^/[StorageFacility] /" >&2) > >(sed -u "s/^/[StorageFacility] /") &
(cd /data/keyserver; /app/pepKeyServer KeyServer.json) 2> >(sed -u "s/^/[KeyServer] /" >&2) > >(sed -u "s/^/[KeyServer] /") &
(cd /data/transcryptor; /app/pepTranscryptor Transcryptor.json) 2> >(sed -u "s/^/[Transcryptor] /" >&2) > >(sed -u "s/^/[Transcryptor] /") &

sleep 2

(cd /data/accessmanager; /app/pepAccessManager AccessManager.json) 2> >(sed -u "s/^/[AccessManager] /" >&2) > >(sed -u "s/^/[AccessManager] /") &

sleep 2

(cd /data/registrationserver; /app/pepRegistrationServer RegistrationServer.json) 2> >(sed -u "s/^/[RegistrationServer] /" >&2) > >(sed -u "s/^/[RegistrationServer] /") &
(cd /data/authserver; /app/pepAuthserver Authserver.json) 2> >(sed -u "s/^/[Authserver] /" >&2) > >(sed -u "s/^/[Authserver] /") &

# Wait for first job to exit
wait -fn
