#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

if [ -f "$SERVERPID_FILENAME" ]; then
  echo "WARNING: SERVERPID file ($SERVERPID_FILENAME) exists, suggests a server is already running. Please stop server or remove PID file before starting a new server."
  exit
fi

# Stash current working dir to return later
cwd=$(pwd)

# Call servers in the background
cd "$BUILDPATH/cpp/pep/servers/" || exit
./pepServers &

# Back to working path
cd "$cwd" || exit

SERVERPID="$!"

console_update "Server started with PID = ${SERVERPID}."

echo "# Exported SERVERPID for local server maintenance (soft lock file).
SERVERPID=$SERVERPID
#EOF" > "$SERVERPID_FILENAME"
