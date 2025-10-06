#!/usr/bin/env bash
set -eu

# Quickly build weblib, start pepServers, server pep-sample-client
# Also watch JS sources so that you only need to rebuild pepWeblib if C++ changes,
#  other builds happen automatically
# You need to open http://localhost:2280/weblib/pep-sample-client/ in browser

cd -- "$(dirname -- "$0")"
foss_dir="$PWD/../.."

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

# Build servers
(
  cd "$foss_dir"
  cmake --build --preset conan-debug --target pepServers
)

# Initial weblib build incl. C++, place symlinks in source directory
(
  cd "$foss_dir"
  cmake --build --preset wasm32-debug --target pepWeblibSampleClient
)

# Start servers
(
  cd "$foss_dir/build/Debug/cpp/pep/servers/"
  ./pepServers
) &
../pep-repo-client-lib/start_websocket_proxy.sh &
./start_nginx.sh

# Start watching
(
  cd ../pep-repo-client-lib/
  npm run build:watch
) &
npm run build:watch &
npm run types:check:watch &

# Wait for first job to exit
wait -fn
