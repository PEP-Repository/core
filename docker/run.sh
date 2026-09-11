#!/usr/bin/env bash
set -eu

while [ $# -gt 0 ]; do
  case "$1" in
    --loglevel) shift
      loglevel="$1" ;;
    -?*)
      >&2 echo "Unknown option: $1"
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

(cd /data/storagefacility; /app/pepStorageFacility "${bin_args[@]}" StorageFacility.json) 2> >(sed -u "s/^/[StorageFacility] /" >&2) > >(sed -u "s/^/[StorageFacility] /") &
(cd /data/keyserver; /app/pepKeyServer "${bin_args[@]}" KeyServer.json) 2> >(sed -u "s/^/[KeyServer] /" >&2) > >(sed -u "s/^/[KeyServer] /") &
(cd /data/transcryptor; /app/pepTranscryptor "${bin_args[@]}" Transcryptor.json) 2> >(sed -u "s/^/[Transcryptor] /" >&2) > >(sed -u "s/^/[Transcryptor] /") &

sleep 2

(cd /data/accessmanager; /app/pepAccessManager "${bin_args[@]}" AccessManager.json) 2> >(sed -u "s/^/[AccessManager] /" >&2) > >(sed -u "s/^/[AccessManager] /") &

sleep 2

(cd /data/registrationserver; /app/pepRegistrationServer "${bin_args[@]}" RegistrationServer.json) 2> >(sed -u "s/^/[RegistrationServer] /" >&2) > >(sed -u "s/^/[RegistrationServer] /") &
(cd /data/authserver; /app/pepAuthserver "${bin_args[@]}" Authserver.json) 2> >(sed -u "s/^/[Authserver] /" >&2) > >(sed -u "s/^/[Authserver] /") &

# Wait for first job to exit
wait -fn
