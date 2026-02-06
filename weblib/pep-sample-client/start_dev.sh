#!/usr/bin/env bash
set -eu

# Quickly build weblib, start pepServers, server pep-sample-client and spin up web server
# Assumes you performed conan install & CMake configure first,
#  with -DPKI_DIR on the wasm32 build set to the pki/ dir in the build folder of the server
# Also watch JS sources so that you only need to rebuild pepWeblib if C++ changes,
#  other builds happen automatically
# You need to open http://localhost:2280/weblib/pep-sample-client/ in browser

if [ "${1-}" = --help ]; then
  echo 'Build already-configured Weblib + servers and spin up server at http://localhost:2280/weblib/pep-sample-client/'
  echo "Usage: $0 [WASM build type (def. Debug)] [pepServers build type (def. WASM build type)] [WASM subbuild name (def. wasm32)]"
  exit
fi

scriptdir="$(realpath -- "$(dirname -- "$0")")"

# E.g. Debug/Release/RelWithDebInfo
wasm_build_type="${1:-Debug}"
server_build_type="${2:-$wasm_build_type}"
wasm_subbuild_name="${3:-wasm32}"

wasm_build_type_lower=$(echo "$wasm_build_type" | tr '[:upper:]' '[:lower:]')
server_build_type_lower=$(echo "$server_build_type" | tr '[:upper:]' '[:lower:]')

foss_dir="$scriptdir/../../"
wasm_build_folder="$foss_dir/build/$wasm_subbuild_name/$wasm_build_type/"
server_build_folder="$foss_dir/build/$server_build_type/"

cd -- "$scriptdir"

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
  cd -- "$foss_dir"
  cmake --build --preset "conan-$server_build_type_lower" --target pepServers
)

# Initial weblib build incl. C++, place symlinks in source directory
(
  cd -- "$foss_dir"
  cmake --build --preset "$wasm_subbuild_name-$wasm_build_type_lower" --target pepWeblibSampleClient
)

# Start servers
(
  cd -- "$server_build_folder/cpp/pep/servers/"
  ./pepServers --loglevel debug
) &
../pep-repo-client-lib/start_websocket_proxy.sh &
../start_nginx.sh "$wasm_build_folder"

# Start watching
(
  cd ../pep-repo-client-lib/
  npm run build:watch
) &
npm run build:watch &
npm run types:check:watch &

# Wait for first job to exit
wait -fn
